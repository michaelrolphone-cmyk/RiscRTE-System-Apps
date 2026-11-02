#!/usr/bin/env python3
"""Apply an explicitly reviewed Home overlay to the cumulative delivered cohort.
No product defaults, inferred bindings, store resizing, or alias shim generation.
This writes unpacked candidates; owning qualification must pack and validate them.
"""
import argparse, copy, hashlib, json
from pathlib import Path, PurePosixPath
from compose_shared_app_cohort import tree, safe, encode, digest, write_tree

def inventory(entries, sd):
 boot=json.loads(entries['boot.json'])
 manifests={}
 for row in boot.get('drivers',[])+boot.get('app_capabilities',[]):
  name=safe(row['manifest'])
  if name not in entries:raise ValueError('Missing installed manifest '+name)
  manifests[name]=json.loads(entries[name])
 return {'files':{n:digest(b) for n,b in sorted(entries.items())},
         'sd_files':{n:digest(b) for n,b in sorted(sd.items())},
         'boot':boot,'manifests':manifests}

def read_lane(cohort,lane):
 entries=tree(cohort/lane/'after')
 path=cohort/lane/'after-sd'
 if path.is_dir():sd=tree(path)
 else:
  receipt=json.loads((cohort/'composition.json').read_text())
  if receipt['products'][lane]['sd_files']!={}:raise ValueError('Missing SD inventory')
  sd={}
 return entries,sd

def fingerprint(record):
 return digest(encode(record))

def declarations(manifest,field):
 rows=manifest.get(field,[])
 if not isinstance(rows,list):raise ValueError('Invalid '+field)
 seen=set()
 for r in rows:
  key=(r['capability'],r['api'])
  if key in seen:raise ValueError('Duplicate declaration '+repr(key))
  seen.add(key)
 return seen

def providers(entries,boot):
 result={}
 seen=set()
 for row in boot.get('drivers',[]):
  path=safe(row['manifest']);instance=row.get('instance_id',0)
  ident=(path,instance)
  if ident in seen:raise ValueError('Duplicate driver instance '+repr(ident))
  seen.add(ident)
  m=json.loads(entries[path])
  for cap,api in declarations(m,'provides'):
   key=(cap,api,instance)
   if key in result:raise ValueError('Duplicate capability provider '+repr(key))
   result[key]=path
 return result

def overlay(before, before_sd, plan, elf, common, packages):
 original=inventory(before,before_sd)
 if plan['inventory_sha256']!=fingerprint(original):raise ValueError('Reviewed inventory differs')
 if not elf.startswith(b'\x7fELF'):raise ValueError('Home is not an ELF')
 if common.get('entry')!='app_main' or common.get('type')!='application':raise ValueError('Invalid complete Home manifest')
 declarations(common,'requires');declarations(common,'optional')
 if declarations(common,'requires')&declarations(common,'optional'):raise ValueError('Overlapping Home declarations')
 out=dict(before);sd=dict(before_sd);boot=copy.deepcopy(original['boot'])
 aliases=plan['aliases']
 if not aliases:raise ValueError('No reviewed Home aliases')
 names=set();routes=set()
 for alias in aliases:
  name=safe(alias['manifest'])
  if name in names:raise ValueError('Duplicate alias')
  names.add(name)
  old=json.loads(before[name])
  if alias.get('role')!='controller':raise ValueError('Only reviewed controller routes may receive Home ELF')
  if old.get('id')=='twatch-clock-return':raise ValueError('Clock return trampoline must remain unchanged')
  if old.get('type')!='application':raise ValueError('Alias is not an installed application')
  # Preserve installed IDs, labels, icons, launch paths and all unrelated metadata.
  new=copy.deepcopy(old)
  for key in ('requires','optional','entry','architecture','version'):
   if key in common:new[key]=copy.deepcopy(common[key])
  if not old.get('file_name'):raise ValueError('Alias lacks executable filename')
  route=alias['route'];path=safe(route['path']);storage=route['storage']
  if storage not in ('internal','sd'):raise ValueError('Unknown executable storage')
  rows=[r for r in boot.get('app_capabilities',[]) if r.get('manifest')==name]
  if rows!=alias['original_policy_rows']:raise ValueError('Alias authority differs')
  if not rows:raise ValueError('Alias has no explicit authority')
  internal=str(PurePosixPath(name).parent/old['file_name'])
  if storage=='internal' and path!=internal:raise ValueError('Executable differs from installed manifest')
  if storage=='sd' and any(r.get('image_source',{}).get('path')!=path for r in rows):
   raise ValueError('SD route differs from installed authority')
  target=out if storage=='internal' else sd
  if path not in target:raise ValueError('Installed executable missing')
  target[path]=elf;routes.add((storage,path));out[name]=encode(new)
  proposed=alias['policy_rows']
  if len(proposed)!=len(rows):raise ValueError('Alias authority row count changed')
  for oldrow,newrow in zip(rows,proposed):
   a=copy.deepcopy(oldrow);b=copy.deepcopy(newrow)
   oldgrants=a.pop('grants',[]);newgrants=b.pop('grants',[])
   if a!=b:raise ValueError('Only explicit additive Home grants are permitted')
   if any(g not in newgrants for g in oldgrants):raise ValueError('Existing grant removed')
   if len({encode(g) for g in newgrants})!=len(newgrants):raise ValueError('Duplicate grant')
   boot['app_capabilities'][boot['app_capabilities'].index(oldrow)]=copy.deepcopy(newrow)
 # Provider packages and exact binding rows come only from the reviewed plan.
 # Existing provider bytes/rows cannot be silently replaced.
 for item in plan.get('providers',[]):
  package=packages/safe(item['package'])
  mbytes=(package/'manifest.json').read_bytes();pbytes=(package/'driver.elf').read_bytes()
  if digest(mbytes)!=item['manifest_sha256'] or digest(pbytes)!=item['elf_sha256']:
   raise ValueError('Provider package differs')
  m=json.loads(mbytes)
  if m.get('type')!='driver' or m.get('file_name')!='driver.elf' or not pbytes.startswith(b'\x7fELF'):
   raise ValueError('Invalid provider package')
  path=safe(item['row']['manifest']);binary=str(PurePosixPath(path).parent/'driver.elf')
  if path in out or binary in out:raise ValueError('Provider would overwrite installed files')
  out[path]=mbytes;out[binary]=pbytes
  boot.setdefault('drivers',[]).append(copy.deepcopy(item['row']))
 providers(out,boot)
 # Top-level namespace maps and every unrelated app/driver field are unchanged.
 out['boot.json']=encode(boot)
 for name in before:
  if name not in names|{'boot.json'} and ('internal',name) not in routes and out[name]!=before[name]:
   raise ValueError('Unrelated internal bytes changed')
 for name in before_sd:
  if ('sd',name) not in routes and sd[name]!=before_sd[name]:raise ValueError('Unrelated SD bytes changed')
 receipt={'inventory_sha256':fingerprint(original),'home_sha256':digest(elf),
          'aliases':sorted(names),'routes':[list(r) for r in sorted(routes)],
          'changed_internal':[n for n in sorted(out) if out[n]!=before.get(n)],
          'changed_sd':[n for n in sorted(sd) if sd[n]!=before_sd.get(n)],
          'packed':False,'dependency_closure_verified':False,'target_execution':False}
 return out,sd,receipt

def run(cohort,plan_path,home,packages,output):
 if output.exists():raise ValueError('Output exists')
 plan=json.loads(plan_path.read_text())
 elf=(home/'default.elf').read_bytes();common=json.loads((home/'default.json').read_text())
 build=json.loads((home/'build.json').read_text())
 if build.get('shared_elf')!=digest(elf) or set(build.get('views',[]))!={'native-faces','overview-retained-desk'}:
  raise ValueError('Missing complete dual-view Home build receipt; alias shims are not Home')
 if build.get('manifest_sha256')!=digest((home/'default.json').read_bytes()):raise ValueError('Home manifest receipt differs')
 results={};prepared={}
 for lane in ('watch','wave','x4'):
  before,sd=read_lane(cohort,lane)
  prepared[lane]=overlay(before,sd,plan[lane],elf,common,packages)
 # Validate every lane before writing any candidate.
 output.mkdir(parents=True)
 for lane,(after,sd,receipt) in prepared.items():
  write_tree(output/lane/'after',after);write_tree(output/lane/'after-sd',sd)
  results[lane]=receipt
 (output/'assembly.json').write_bytes(encode({'products':results,'byte_identical_home':digest(elf),'packed':False}))
 return results

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--cohort',type=Path,required=True);p.add_argument('--inventory-output',type=Path)
 for name in ('plan','home','packages','output'):p.add_argument('--'+name,type=Path)
 a=p.parse_args()
 if a.inventory_output:
  records={lane:inventory(*read_lane(a.cohort,lane)) for lane in ('watch','wave','x4')}
  a.inventory_output.write_bytes(encode({lane:{'sha256':fingerprint(v),'inventory':v} for lane,v in records.items()}))
 else:
  if not all((a.plan,a.home,a.packages,a.output)):p.error('--plan, --home, --packages and --output required for overlay')
  run(a.cohort,a.plan,a.home,a.packages,a.output)

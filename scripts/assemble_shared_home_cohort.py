#!/usr/bin/env python3
"""Apply an explicitly reviewed Home overlay to the cumulative delivered cohort.
No product defaults, inferred bindings, store resizing, or alias shim generation.
This writes unpacked candidates; owning qualification must pack and validate them.
"""
import argparse, copy, hashlib, json, sys, zipfile
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
         'boot':boot,'manifests':manifests,
         'deployment_metadata':{n:json.loads(entries[n]) for n in ('cohort.json',) if n in entries}}

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

def checked_native(packages,package,descriptor_sha256):
 root=packages/safe(package)
 raw=(root/'native.json').read_bytes()
 if digest(raw)!=descriptor_sha256:raise ValueError('Native descriptor differs')
 native=json.loads(raw)
 for key in ('source_revision','runtime_revision'):
  value=native.get(key)
  if not isinstance(value,str) or len(value)!=40 or any(c not in '0123456789abcdef' for c in value):
   raise ValueError('Invalid native source revision')
 if not isinstance(native.get('runtime_version'),str) or not native['runtime_version']:
  raise ValueError('Missing native runtime version')
 for key in ('firmware','qualification'):
  item=native[key];data=(root/safe(item['path'])).read_bytes()
  if type(item['bytes']) is not int or item['bytes']<=0 or len(data)!=item['bytes'] or digest(data)!=item['sha256']:
   raise ValueError('Native '+key+' bytes differ')
  if key=='firmware' and ('RISC_RUNTIME_VERSION:'+native['runtime_version']).encode()+b'\0' not in data:
   raise ValueError('Native firmware runtime marker differs')
  if key=='qualification':json.loads(data)
 return native

def replace_native_metadata(entries,spec,packages):
 if spec['path']!='cohort.json':raise ValueError('Only existing cohort deployment metadata may be replaced')
 oldbytes=entries['cohort.json']
 if digest(oldbytes)!=spec['before_sha256']:raise ValueError('Native metadata baseline differs')
 native=checked_native(packages,spec['package'],spec['descriptor_sha256'])
 values={'firmware_size':native['firmware']['bytes'],'firmware_sha256':native['firmware']['sha256'],
         'runtime_version':native['runtime_version'],'version':spec['product_version'],
         'source_revision':native['source_revision'],'runtime_revision':native['runtime_revision']}
 fields=spec['fields']
 if not {'firmware_size','firmware_sha256','runtime_version','version'}<=set(fields) or not set(fields)<=set(values):
  raise ValueError('Incomplete or unknown native metadata binding')
 if not isinstance(spec['product_version'],str) or not spec['product_version']:raise ValueError('Missing candidate product version')
 old=json.loads(oldbytes);new=copy.deepcopy(old);seen=set()
 for semantic,binding in fields.items():
  path=binding['path']
  if not isinstance(path,list) or not path or any(not isinstance(k,str) or not k for k in path):
   raise ValueError('Invalid existing metadata field path')
  key=tuple(path)
  if key in seen or any(key[:len(p)]==p or p[:len(key)]==key for p in seen):raise ValueError('Overlapping metadata field bindings')
  seen.add(key);node=new
  for part in path[:-1]:
   if not isinstance(node,dict) or part not in node:raise ValueError('Metadata field absent')
   node=node[part]
  if not isinstance(node,dict) or path[-1] not in node or node[path[-1]]!=binding['before']:
   raise ValueError('Metadata field baseline differs')
  node[path[-1]]=values[semantic]
 entries['cohort.json']=encode(new)
 return {'path':'cohort.json','before_sha256':digest(oldbytes),'after_sha256':digest(entries['cohort.json']),
         'descriptor_sha256':spec['descriptor_sha256'],'native':native,'product_version':spec['product_version']}

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
 provider_paths=set();provider_sd_paths=set()
 for item in plan.get('providers',[]):
  package=packages/safe(item['package'])
  mbytes=(package/'manifest.json').read_bytes();pbytes=(package/'driver.elf').read_bytes()
  if digest(mbytes)!=item['manifest_sha256'] or digest(pbytes)!=item['elf_sha256']:
   raise ValueError('Provider package differs')
  m=json.loads(mbytes)
  if m.get('type')!='driver' or m.get('file_name')!='driver.elf' or not pbytes.startswith(b'\x7fELF'):
   raise ValueError('Invalid provider package')
  path=safe(item['row']['manifest']);binary=str(PurePosixPath(path).parent/'driver.elf')
  replacement=item.get('replace')
  if replacement is None:
   if item['row'].get('image_source'):raise ValueError('New external provider routes require a separate reviewed installation')
   if path in out or binary in out:raise ValueError('Provider would overwrite installed files')
   boot.setdefault('drivers',[]).append(copy.deepcopy(item['row']))
  else:
   if path not in out or binary not in out:raise ValueError('Replacement provider missing')
   if digest(out[path])!=replacement['manifest_sha256'] or digest(out[binary])!=replacement['elf_sha256']:
    raise ValueError('Replacement provider baseline differs')
   existing=[r for r in boot.get('drivers',[]) if r.get('manifest')==path]
   if not existing or any(r!=item['row'] for r in existing):
    raise ValueError('Replacement provider authority differs')
   source=item['row'].get('image_source')
   external=replacement.get('external_route')
   if source:
    if not isinstance(source,dict) or source.get('internal_fallback') is not True:
     raise ValueError('Replacement requires the installed internal fallback contract')
    if not isinstance(external,dict) or set(external)!={'path','elf_sha256'}:
     raise ValueError('Explicit external provider baseline required')
    external_path=safe(external['path'])
    if external_path!=source.get('path'):raise ValueError('External provider route differs')
    if external_path not in sd or digest(sd[external_path])!=external['elf_sha256']:
     raise ValueError('External provider baseline differs')
    if external_path in provider_sd_paths or ('sd',external_path) in routes:
     raise ValueError('External provider route collision')
    sd[external_path]=pbytes;provider_sd_paths.add(external_path)
   elif external is not None:
    raise ValueError('External route supplied for internal-only provider')
   previous=json.loads(out[path])
   if {k:v for k,v in previous.items() if k!='version'}!={k:v for k,v in m.items() if k!='version'}:
    raise ValueError('Replacement provider changes selected profile or contract')
  out[path]=mbytes;out[binary]=pbytes;provider_paths.update((path,binary))
 providers(out,boot)
 # Top-level namespace maps and every unrelated app/driver field are unchanged.
 out['boot.json']=encode(boot)
 for name in before:
  if name not in names|provider_paths|{'boot.json'} and ('internal',name) not in routes and out[name]!=before[name]:
   raise ValueError('Unrelated internal bytes changed')
 for name in before_sd:
  if name not in provider_sd_paths and ('sd',name) not in routes and sd[name]!=before_sd[name]:raise ValueError('Unrelated SD bytes changed')
 updates=[]
 for item in plan.get('app_updates',[]):
  name=safe(item['manifest'])
  if name in names:raise ValueError('Independent app must not be a Home controller alias')
  package=packages/safe(item['package'])
  mbytes=(package/safe(item['manifest_file'])).read_bytes()
  abytes=(package/safe(item['elf_file'])).read_bytes()
  if digest(mbytes)!=item['manifest_sha256'] or digest(abytes)!=item['elf_sha256']:
   raise ValueError('Independent application package differs')
  old=json.loads(out[name]);new=json.loads(mbytes)
  if old.get('id')!=new.get('id'):raise ValueError('Installed application identity changed')
  route=item['route'];target=out if route['storage']=='internal' else sd
  if digest(out[name])!=item['replace']['manifest_sha256'] or digest(target[safe(route['path'])])!=item['replace']['elf_sha256']:
   raise ValueError('Independent application baseline differs')
  alias={key:copy.deepcopy(item[key]) for key in ('manifest','route','original_policy_rows','policy_rows')}
  alias['role']='controller'
  subplan={'inventory_sha256':fingerprint(inventory(out,sd)),'aliases':[alias]}
  out,sd,_=overlay(out,sd,subplan,abytes,new,packages)
  updates.append({'manifest':name,'elf_sha256':digest(abytes),'package_manifest_sha256':digest(mbytes),
                  'installed_manifest_sha256':digest(out[name])})
 native_metadata=replace_native_metadata(out,plan['native_metadata'],packages) if 'native_metadata' in plan else None
 receipt={'inventory_sha256':fingerprint(original),'home_sha256':digest(elf),
          'native_metadata':native_metadata,'aliases':sorted(names),'application_updates':updates,'routes':[list(r) for r in sorted(routes)],
          'changed_internal':[n for n in sorted(out) if out[n]!=before.get(n)],
          'changed_sd':[n for n in sorted(sd) if sd[n]!=before_sd.get(n)],
          'packed':False,'dependency_closure_verified':False,'target_execution':False}
 return out,sd,receipt

def packing_inputs(inputs,tools,cohort):
 # Reuse the already delivered Files store decoder/packer and exact inputs.
 from derive_files_product_authority import INPUTS,TOOLS,digest as file_digest,read_member
 for name in ('watch.zip','wave.zip','x4-store.bin'):
  if file_digest(inputs/name)!=INPUTS[name]:raise ValueError('Original product input differs '+name)
 for name,expected in TOOLS.items():
  if file_digest(tools/name)!=expected:raise ValueError('Owning bootfs tool differs '+name)
 sys.path.insert(0,str(tools.resolve()))
 from read_only_spiffs import read_image
 from current_bootfs import build
 sizes={}
 for lane in ('watch','wave','x4'):
  raw=(inputs/'x4-store.bin').read_bytes() if lane=='x4' else read_member(inputs/(lane+'.zip'),'candidate/store.bin')
  if read_image(raw,len(raw))!=tree(cohort/lane/'before'):raise ValueError('Original inventory differs '+lane)
  sizes[lane]=len(raw)
 return sizes,build,read_image

def validate_bootfs_names(names):
 # Existing owning SPIFFS ABI: 32 bytes includes leading slash and NUL.
 for name in names:
  safe(name)
  if not name.isascii() or len(('/'+name).encode())>=32:raise ValueError('Installed bootfs path exceeds existing name field: '+name)

def pack_candidate(directory,entries,sd,size,build,read_image):
 validate_bootfs_names(entries)
 image,packing=build(entries,size)
 if len(image)!=size or read_image(image,size)!=entries:raise ValueError('Store roundtrip differs')
 (directory/'store.bin').write_bytes(image)
 # Canonical ZIP metadata; stored member bytes must remain exactly the candidate.
 path=directory/'sd-apps.zip'
 with zipfile.ZipFile(path,'w',compression=zipfile.ZIP_DEFLATED) as z:
  for name,data in sorted(sd.items()):
   info=zipfile.ZipInfo(safe(name),(1980,1,1,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED
   info.external_attr=0o100644<<16;z.writestr(info,data)
 with zipfile.ZipFile(path) as z:
  if {n:z.read(n) for n in z.namelist()}!=sd:raise ValueError('SD roundtrip differs')
 return {'packed':True,'store_bytes':size,'store_sha256':digest(image),
         'sd_archive_sha256':digest(path.read_bytes()),'packing':packing,
         'store_roundtrip':True,'sd_roundtrip':True}

def run(cohort,plan_path,home,packages,output,inputs=None,bootfs_tools=None):
 if output.exists():raise ValueError('Output exists')
 plan=json.loads(plan_path.read_text())
 elf=(home/'default.elf').read_bytes();common=json.loads((home/'default.json').read_text())
 build=json.loads((home/'build.json').read_text())
 if build.get('shared_elf')!=digest(elf) or set(build.get('views',[]))!={'native-faces','overview-retained-desk'}:
  raise ValueError('Missing complete dual-view Home build receipt; alias shims are not Home')
 if build.get('manifest_sha256')!=digest((home/'default.json').read_bytes()):raise ValueError('Home manifest receipt differs')
 if bool(inputs)!=bool(bootfs_tools):raise ValueError('Both original inputs and pinned bootfs tools required')
 packing=packing_inputs(inputs,bootfs_tools,cohort) if inputs else None
 results={};prepared={}
 for lane in ('watch','wave','x4'):
  before,sd=read_lane(cohort,lane)
  prepared[lane]=overlay(before,sd,plan[lane],elf,common,packages)
 # Validate every lane before writing any candidate.
 output.mkdir(parents=True)
 for lane,(after,sd,receipt) in prepared.items():
  write_tree(output/lane/'before',tree(cohort/lane/'before'))
  write_tree(output/lane/'after',after);write_tree(output/lane/'after-sd',sd)
  if packing:
   sizes,build,read_image=packing
   receipt.update(pack_candidate(output/lane,after,sd,sizes[lane],build,read_image))
  results[lane]=receipt
 (output/'assembly.json').write_bytes(encode({'products':results,'byte_identical_home':digest(elf),'packed':bool(packing)}))
 return results

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--cohort',type=Path,required=True);p.add_argument('--inventory-output',type=Path)
 p.add_argument('--inputs',type=Path);p.add_argument('--bootfs-tools',type=Path)
 for name in ('plan','home','packages','output'):p.add_argument('--'+name,type=Path)
 a=p.parse_args()
 if a.inventory_output:
  records={lane:inventory(*read_lane(a.cohort,lane)) for lane in ('watch','wave','x4')}
  a.inventory_output.write_bytes(encode({lane:{'sha256':fingerprint(v),'inventory':v} for lane,v in records.items()}))
 else:
  if not all((a.plan,a.home,a.packages,a.output)):p.error('--plan, --home, --packages and --output required for overlay')
  run(a.cohort,a.plan,a.home,a.packages,a.output,a.inputs,a.bootfs_tools)

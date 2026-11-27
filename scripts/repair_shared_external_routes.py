#!/usr/bin/env python3
"""Review/apply route-only repairs to an exact unpacked cumulative product.

No executable, contract, grant, namespace, private data, native firmware or store
geometry is changed. Existing SD objects are retained; a corrupt object already
at the desired content address is rejected. No device access or publication.
"""
import argparse,copy,json,re
from pathlib import Path,PurePosixPath
from assemble_shared_home_cohort import inventory,fingerprint,write_external,same_json
from compose_shared_app_cohort import digest,encode,safe,tree,write_tree

def binding(entries,row,section):
 manifest_path=safe(row['manifest']);raw=entries[manifest_path];manifest=json.loads(raw)
 kind,directory=('driver','Drivers') if section=='drivers' else ('application','Apps')
 if manifest.get('type')!=kind:raise ValueError('Selected manifest type differs')
 ident=manifest.get('id','');filename=manifest.get('file_name','')
 if len(ident)>=64 or not re.fullmatch(r'[a-z0-9](?:[a-z0-9_-]*[a-z0-9])?',ident):raise ValueError('Unsafe package identity')
 if not 4<len(filename)<128 or not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*\.elf',filename) or '..' in filename:
  raise ValueError('Unsafe executable filename')
 binary=str(PurePosixPath(manifest_path).parent/filename)
 if any('\0' in name or len(name.encode())>=193 for name in (manifest_path,binary)):
  raise ValueError('Runtime path buffer bound exceeded')
 data=entries[binary]
 if not data.startswith(b'\x7fELF'):raise ValueError('Installed fallback is not ELF')
 source=row.get('image_source')
 if not isinstance(source,dict) or source.get('internal_fallback') is not True:
  raise ValueError('Selected route has no installed internal fallback')
 path=safe(source['path']);prefix=directory+'/'+ident+'/image-'
 if not re.fullmatch(re.escape(prefix)+r'[0-9a-f]{64}\.elf',path):
  raise ValueError('Installed route is not a preserving-policy content path')
 return manifest_path,binary,data,path,prefix+digest(data)+'.elf'

def prepare(entries,sd):
 boot=json.loads(entries['boot.json']);items=[]
 for section in ('drivers','app_capabilities'):
  for index,row in enumerate(boot.get(section,[])):
   if row.get('image_source',{}).get('internal_fallback') is not True:continue
   manifest,binary,data,path,target=binding(entries,row,section)
   # An absent preferred image is valid when the installed fallback is already
   # correctly addressed. Do not invent an SD installation for that route.
   if path==target and (path not in sd or sd[path]==data):continue
   if path not in sd:raise ValueError('Mismatched preferred image is missing')
   items.append({'section':section,'index':index,'row':copy.deepcopy(row),
     'manifest_sha256':digest(entries[manifest]),'internal_path':binary,'internal_sha256':digest(data),
     'external_route':{'path':path,'elf_sha256':digest(sd[path]),'target_path':target}})
 return {'schema':1,'inventory_sha256':fingerprint(inventory(entries,sd)),'repairs':items}

def apply(entries,sd,plan):
 if plan.get('schema')!=1 or plan['inventory_sha256']!=fingerprint(inventory(entries,sd)):
  raise ValueError('Reviewed repair inventory differs')
 out=dict(entries);external=dict(sd);boot=copy.deepcopy(json.loads(entries['boot.json']));seen=set();changed=[]
 for item in plan['repairs']:
  section,index=item['section'],item['index']
  if section not in ('drivers','app_capabilities') or type(index) is not int or index<0:
   raise ValueError('Invalid selected authority row')
  if (section,index) in seen:raise ValueError('Duplicate repair')
  seen.add((section,index));rows=boot.get(section,[])
  if index>=len(rows) or not same_json(rows[index],item['row']):raise ValueError('Selected authority differs')
  row=rows[index];manifest,binary,data,path,target=binding(entries,row,section)
  route=item['external_route']
  if item['manifest_sha256']!=digest(entries[manifest]) or item['internal_path']!=binary or item['internal_sha256']!=digest(data):
   raise ValueError('Installed fallback baseline differs')
  if set(route)!={'path','elf_sha256','target_path'} or route['path']!=path or route['target_path']!=target:
   raise ValueError('Unreviewed repair destination')
  if path not in sd or digest(sd[path])!=route['elf_sha256']:raise ValueError('Preferred image baseline differs')
  write_external(external,boot,route,data,[(section,row)])
  row['image_source']['path']=target
  changed.append({'section':section,'index':index,'manifest':manifest,'path':path,'target_path':target,'elf_sha256':digest(data)})
 if changed:out['boot.json']=encode(boot)
 # A complete repair plan must cover all remaining inconsistent fallback routes.
 if prepare(out,external)['repairs']:raise ValueError('Repair plan leaves inconsistent fallback routes')
 receipt={'schema':1,'inventory_sha256':plan['inventory_sha256'],'result_inventory_sha256':fingerprint(inventory(out,external)),
   'repairs':changed,'changed_internal':[n for n in sorted(out) if out[n]!=entries.get(n)],
   'changed_sd':[n for n in sorted(external) if external[n]!=sd.get(n)],
   'removed_internal':[],'removed_sd':[],'hardware_tested':False,'runtime_policy_tested':False}
 return out,external,receipt

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('operation',choices=('plan','apply'));p.add_argument('--internal',type=Path,required=True)
 p.add_argument('--sd',type=Path,required=True);p.add_argument('--plan',type=Path,required=True);p.add_argument('--output',type=Path)
 a=p.parse_args();entries=tree(a.internal);sd=tree(a.sd)
 if a.operation=='plan':
  if a.plan.exists():raise ValueError('Refusing to replace reviewed plan')
  a.plan.write_bytes(encode(prepare(entries,sd)))
 else:
  if not a.output or a.output.exists():raise ValueError('Fresh output required')
  out,external,receipt=apply(entries,sd,json.loads(a.plan.read_text()))
  write_tree(a.output/'after',out);write_tree(a.output/'after-sd',external)
  (a.output/'repair.json').write_bytes(encode(receipt))
if __name__=='__main__':main()

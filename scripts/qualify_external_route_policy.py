#!/usr/bin/env python3
"""Run actual unchanged Runtime policy against real repaired X4 inventories."""
import argparse,copy,json,shutil,subprocess,hashlib
from pathlib import Path
from compose_shared_app_cohort import tree,encode,write_tree,digest

def qualify(root):
 root=root.resolve();before=root/'repaired-final/x4/before';after=root/'repaired-final/x4/after'
 out=root/'policy/selected-profile/results';out.mkdir(parents=True,exist_ok=False)
 original=tree(before);candidate=tree(after)
 rows=[]
 cases=[('baseline',original,1,0),('repaired',candidate,1,1)]
 for fault in ('old-path','volume','float-volume','fallback','namespace','wrong-id','manifest','elf','inventory-hash'):
  values=dict(candidate);boot=json.loads(values['boot.json']);driver=next(r for r in boot['drivers'] if r['manifest']=='power/manifest.json')
  if fault=='old-path':driver['image_source']['path']=next(r for r in json.loads(original['boot.json'])['drivers'] if r['manifest']=='power/manifest.json')['image_source']['path']
  elif fault=='volume':driver['image_source']['volume_instance']+=1
  elif fault=='float-volume':driver['image_source']['volume_instance']=float(driver['image_source']['volume_instance'])
  elif fault=='fallback':driver['image_source']['internal_fallback']=False
  elif fault=='namespace':boot['unexpected_namespace']=123
  elif fault=='wrong-id':driver['image_source']['path']=driver['image_source']['path'].replace('/x4pro-power/','/unrelated/')
  elif fault=='manifest':
   m=json.loads(values['power/manifest.json']);m['unexpected']=True;values['power/manifest.json']=encode(m)
  elif fault=='elf':values['power/driver.elf']+=b'tampered'
  values['boot.json']=encode(boot)
  policy=int(fault in ('old-path','elf','inventory-hash'))
  images=int(fault not in ('old-path','wrong-id','elf','inventory-hash'))
  cases.append((fault,values,policy,images))
 for name,values,policy,images in cases:
  directory=out/name;write_tree(directory/'candidate',values)
  files=[{'path':n,'bytes':len(b),'sha256':digest(b)} for n,b in sorted(values.items())]
  if name=='inventory-hash':next(f for f in files if f['path']=='power/driver.elf')['sha256']='0'*64
  profile=directory/'profile.json';profile.write_bytes(encode({'files':files}))
  for mode in ('normal','ubsan'):
   cmd=[str(root/'policy/selected-profile'/mode),str(before),str(directory/'candidate'),str(profile),str(policy),str(images)]
   run=subprocess.run(cmd,capture_output=True,text=True,timeout=15)
   (directory/(mode+'.log')).write_text(run.stdout+run.stderr)
   if run.returncode:raise RuntimeError(name+'/'+mode+' unexpected policy result: '+run.stdout+run.stderr)
   result=json.loads(run.stdout);rows.append({'case':name,'mode':mode,'result':result})
 assert tree(before)==original and tree(after)==candidate
 receipt={'passed':True,'cases':rows,'runtime_revision':'0052d1fc42e9cfc82d626c5a247d460919c87935',
   'runtime_policy_source_unchanged':True,'hardware_tested':False,'full_graph_admission':False,
   'scope':'Actual production preservingPolicy and preservingImageSources; unchanged file-source test backend, real decoded candidate stores and generated exact profile inventories. No SD/device I/O or activation.'}
 (out/'receipt.json').write_bytes(encode(receipt));print(json.dumps(receipt),flush=True)
 return receipt

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);a=p.parse_args();qualify(a.root)

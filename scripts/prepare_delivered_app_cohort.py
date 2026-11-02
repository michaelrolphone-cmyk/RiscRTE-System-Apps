#!/usr/bin/env python3
"""Verify retained delivery archives and compose their exact independent deltas."""
import argparse, hashlib, io, json, zipfile
from pathlib import Path
from compose_shared_app_cohort import compose,safe
ARTIFACTS={
 'files':('michaelrolphone-cmyk/RiscRTE-System-Apps',13118207859,'a9fb7559629ee0b9a7f0f6f43cd6b385bdfa92c1355ea454a3cd8a02bf3fc8b0'),
 'lists':('michaelrolphone-cmyk/RiscRTE-Productivity',13120780374,'e47768fe5cb6977c9b4db617eb7d3729cfdd1f24c6756abe3a779e6a15a2851a'),
 'viewer':('michaelrolphone-cmyk/RiscRTE-Utilities',13122108650,'885a709247ed7b13122c89a9eec3070d53c4056efe90138aefb5ad6606798ab3')}
def run(archives,output):
 if output.exists():raise ValueError('Output exists')
 output.mkdir(parents=True);retained=[]
 for label,(repo,ident,digest) in ARTIFACTS.items():
  path=archives/(label+'.zip')
  with path.open('rb') as f:actual=hashlib.file_digest(f,'sha256').hexdigest()
  if actual!=digest:raise ValueError('Published artifact differs '+label)
  with zipfile.ZipFile(path) as z:
   names=z.namelist()
   if len(names)!=len(set(names)):raise ValueError('Duplicate artifact paths')
   for n in names:safe(n)
   if 'products/watch/before/boot.json' not in names:raise ValueError('Unexpected qualification layout '+label)
   for n in names:
    if n.endswith('/'):continue
    if n.startswith('products/'):
     dest=output/label/safe(n);dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(z.read(n))
   if label=='viewer':
    sd=output/'baseline-sd';sd.mkdir()
    (sd/'x4.zip').write_bytes(z.read('inputs/x4-sd.zip'))
    for lane in ('watch','wave'):
     with zipfile.ZipFile(io.BytesIO(z.read('inputs/'+lane+'.zip'))) as source:
      if 'candidate/sd-apps.zip' in source.namelist():(sd/(lane+'.zip')).write_bytes(source.read('candidate/sd-apps.zip'))
  retained.append({'label':label,'repository':repo,'artifact_id':ident,'sha256':actual})
 (output/'sources.json').write_text(json.dumps(retained,indent=2)+'\n')
 baseline={}
 for lane in ('watch','wave','x4'):
  base=output/'files'/'products'/lane/'before'
  baseline[lane]={'boot':json.loads((base/'boot.json').read_text()),'manifests':{str(p.relative_to(base)):json.loads(p.read_text()) for p in base.rglob('*.json') if p.name in ('default.json','manifest.json')}}
 (output/'baseline-inventory.json').write_text(json.dumps(baseline,indent=2)+'\n')
 print('HOME_BASELINE_INVENTORY '+json.dumps(baseline,sort_keys=True),flush=True)
 compose([output/label/'products' for label in ARTIFACTS],output/'baseline-sd',output/'composed')
 inventory={}
 for lane in ('watch','wave','x4'):
  base=output/'composed'/lane/'after'
  boot=json.loads((base/'boot.json').read_text())
  manifests={str(p.relative_to(base)):json.loads(p.read_text()) for p in base.rglob('*.json') if p.name in ('default.json','manifest.json')}
  inventory[lane]={'boot':boot,'manifests':manifests}
 (output/'inventory.json').write_text(json.dumps(inventory,indent=2)+'\n')
 print('HOME_COHORT_INVENTORY '+json.dumps(inventory,sort_keys=True),flush=True)
 (output/'sources.json').write_text(json.dumps(retained,indent=2)+'\n')
 print('Independent Files, Lists and Viewer deltas composed without conflict; no Home acceptance claimed.')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--archives',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
 a=p.parse_args();run(a.archives,a.output)

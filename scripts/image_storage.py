"""Lossless storage transforms for explicitly installed internal ordinary apps.
No native/provider image, mirror, manifest, authority, path or data is changed.
Runtime envelope support must be qualified separately before deployment.
"""
import json
from pathlib import PurePosixPath
from compose_shared_app_cohort import safe,digest
from image_envelope import encode_image,decode_image,receipt
def image_path(entries,manifest_path):
 manifest_path=safe(manifest_path);m=json.loads(entries[manifest_path])
 return m,safe(str(PurePosixPath(manifest_path).parent/m['file_name']))
def ordinary_images(entries):
 boot=json.loads(entries['boot.json']);drivers=set();apps=set();forbidden=set()
 for row in boot.get('drivers',[]):
  _,path=image_path(entries,row['manifest']);drivers.add(path)
 for row in boot.get('app_capabilities',[]):
  manifest,path=image_path(entries,row['manifest'])
  if 'image_source' in row or row.get('catalog_authority'):
   forbidden.add(path);continue
  if manifest.get('type')!='application':raise ValueError('Application policy names non-application')
  if path not in entries:raise ValueError('Missing ordinary application image')
  if path not in drivers:apps.add(path)
 return apps-drivers-forbidden
def candidates(entries):
 result={}
 for path in sorted(ordinary_images(entries)):
  raw=entries[path];encoded=encode_image(raw)
  if len(encoded)<len(raw):result[path]={'encoded':encoded,'receipt':receipt(raw,encoded)}
 return result
def apply(entries,selected,expected):
 if len(selected)!=len(set(selected)):raise ValueError('Duplicate encoded application')
 allowed=ordinary_images(entries);out=dict(entries);records={}
 for path in selected:
  if path not in allowed:raise ValueError('Encoding requires internal ordinary non-mirrored application')
  if expected.get(path)!=digest(entries[path]):raise ValueError('Canonical baseline differs')
  encoded=encode_image(entries[path])
  if len(encoded)>=len(entries[path]):raise ValueError('Encoding does not save storage')
  out[path]=encoded;records[path]=receipt(entries[path],encoded)
 if set(expected)!=set(selected):raise ValueError('Unmatched encoding baseline')
 recovered=dict(out)
 for path in selected:recovered[path]=decode_image(out[path])
 if recovered!=entries:raise ValueError('Canonical store recovery differs')
 return out,{'schema':1,'format':'RTEIMG1','images':records,
             'canonical_files':{n:digest(b) for n,b in sorted(entries.items())},
             'stored_files':{n:digest(b) for n,b in sorted(out.items())},
             'canonical_recovery_verified':True}

def fit(entries,size,build):
 """Choose the shortest descending-savings prefix that the actual owning packer admits."""
 try:
  build(entries,size)
  return dict(entries),None
 except Exception as error:
  if type(error).__name__!='SpiffsFullError':raise
 options=candidates(entries)
 selected=[];expected={}
 for path in sorted(options,key=lambda n:(-(options[n]['receipt']['canonical_bytes']-options[n]['receipt']['stored_bytes']),n)):
  selected.append(path);expected[path]=digest(entries[path])
  stored,record=apply(entries,selected,expected)
  try:build(stored,size)
  except Exception as error:
   if type(error).__name__!='SpiffsFullError':raise
   continue
  record['selection']='shortest descending positive-savings prefix admitted by original fixed-size packer'
  record['selected']=selected
  return stored,record
 raise ValueError('Complete canonical app set cannot fit original store under eligible lossless encoding')

#!/usr/bin/env python3
"""Derive Files authority only from exact released product bytes; never invent grants."""
import argparse, hashlib, json, sys, zipfile
from pathlib import Path, PurePosixPath

INPUTS = {
 'watch.zip':'3a5aadb0eeb5c4b37640d606978162b5c0484893aada661baca4ca39a12d66f2',
 'wave.zip':'aae8665d9240bd49dbdae89f250525ccea6cabc44215133248ee1a2c7b19d1d6',
 'x4-store.bin':'8d38a77bdb3dd1439e8736d9c0363798f20803e02b44fc1aa5dab578207c4adc',
 'x4-sd.zip':'2b6dce3efb468f3cd728b50707e0c9552d84e225b65e2b27e683d3d726bc6cef',
 'watchrtc.zip':'4ab38e133604991f2d09a2739f73dccc5dfa413e94527676795df9667378a671',
 'wavertc.zip':'80fa5c1805f21eb97ee844a495bcde1e00c4c53d5eecf89ac204d64dbbaefa1b',
}
TOOLS = {
 'current_bootfs.py':'6f0997a71fe6d2abbb97ed91e0e4180abf43b2056afbcb5b54d303437791576b',
 'read_only_spiffs.py':'e48d3b2422c60f81fdae6371ff3c5b65c5cbaa9dd250b4a2925c74c0b45f040d',
}
def digest(path):
 with path.open('rb') as f: return hashlib.file_digest(f,'sha256').hexdigest()
def product_record(files):
 boot_bytes=files['boot.json']
 boot=json.loads(boot_bytes)
 rows=[r for r in boot['app_capabilities'] if r.get('manifest')=='file_browser.json']
 if not rows: raise ValueError('Original Files authority is absent')
 manifest=json.loads(files['file_browser.json'])
 return {'boot_sha256':hashlib.sha256(boot_bytes).hexdigest(),
         'rows':rows,'manifests':{'file_browser.json':manifest}}
def read_member(path, member):
 with zipfile.ZipFile(path) as z:
  names=z.namelist()
  if len(names)!=len(set(names)): raise ValueError('Duplicate ZIP names')
  for name in names:
   p=PurePosixPath(name)
   if not name or p.is_absolute() or '..' in p.parts or '\\' in name:
    raise ValueError('Unsafe ZIP name')
  return z.read(member)
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('inputs','bootfs-tools','output'): p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args()
 if a.output.exists(): raise ValueError('Output already exists')
 for n,h in INPUTS.items():
  if digest(a.inputs/n)!=h: raise ValueError('Original input differs: '+n)
 for n,h in TOOLS.items():
  if digest(a.bootfs_tools/n)!=h: raise ValueError('Bootfs tool differs: '+n)
 sys.path.insert(0,str(a.bootfs_tools.resolve()))
 from read_only_spiffs import read_image
 products={}
 for lane in ('watch','wave','x4'):
  raw=(a.inputs/'x4-store.bin').read_bytes() if lane=='x4' else read_member(a.inputs/(lane+'.zip'),'candidate/store.bin')
  products[lane]=product_record(read_image(raw,len(raw)))
 result={'results':{'record':{'payload':{'inline':{'products':products}}}},
         'provenance':{'method':'decoded exact original stores; not fixture authority',
                       'inputs':INPUTS,'bootfs_tools':TOOLS}}
 a.output.parent.mkdir(parents=True,exist_ok=True)
 a.output.write_text(json.dumps(result,sort_keys=True,indent=2)+'\n')
 print('Original authority derived from all six SHA-verified product inputs')
if __name__=='__main__': main()

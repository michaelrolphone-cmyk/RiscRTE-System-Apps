#!/usr/bin/env python3
"""Preserve the published Home closure while replacing only shared Touchpad and ui.scene."""
import argparse,hashlib,json,sys,urllib.request,zipfile,io
from pathlib import Path,PurePosixPath
from compose_shared_app_cohort import safe,encode,digest,write_tree
from image_envelope import MAGIC,decode_image
BASELINE={
  "assembly.json": {
    "url": "https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/releases/download/home-shared-83afd19e5184eb511cc205299c83eb9cb5c05430/assembly.json",
    "sha256": "9c0b333a20bbe95a13e13bcfcfd5ec4aba7fe575a61598f7f36269bc568bacb5",
    "size": 43444
  },
  "home-delivery.json": {
    "url": "https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/releases/download/home-shared-83afd19e5184eb511cc205299c83eb9cb5c05430/home-delivery.json",
    "sha256": "1a8eb4c56c3cbc5faa8edcc38d9592c25b4d7ccb5a77680483544ca73c630dde",
    "size": 37793
  },
  "product-watch-sd-sd-apps.zip": {
    "url": "https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/releases/download/home-shared-83afd19e5184eb511cc205299c83eb9cb5c05430/product-watch-sd-sd-apps.zip",
    "sha256": "8739c76e681f900923b900c9df0ef75cf421d39cabb54650c4b9ad19b6a76d85",
    "size": 22
  },
  "product-watch-store-store.bin": {
    "url": "https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/releases/download/home-shared-83afd19e5184eb511cc205299c83eb9cb5c05430/product-watch-store-store.bin",
    "sha256": "01f19575dd163c9ffe9a29fc660794895ae227b9a5413afb6dc2818695c1e6dd",
    "size": 5570560
  },
  "product-wave-sd-sd-apps.zip": {
    "url": "https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/releases/download/home-shared-83afd19e5184eb511cc205299c83eb9cb5c05430/product-wave-sd-sd-apps.zip",
    "sha256": "9008ae2c1f7cdfb21c301667d0669cd994c287990a638fbd824de1421c349b22",
    "size": 1767408
  },
  "product-wave-store-store.bin": {
    "url": "https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/releases/download/home-shared-83afd19e5184eb511cc205299c83eb9cb5c05430/product-wave-store-store.bin",
    "sha256": "84d83fc6386ead2ccbf3059ea87cd3d23ec80a918d6aef891a4449a449eae293",
    "size": 5570560
  },
  "product-x4-sd-sd-apps.zip": {
    "url": "https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/releases/download/home-shared-83afd19e5184eb511cc205299c83eb9cb5c05430/product-x4-sd-sd-apps.zip",
    "sha256": "c1948739143957840a57ed3d9de4b5f7359d42be93df7b670386c7c56498f59a",
    "size": 5444854
  },
  "product-x4-store-store.bin": {
    "url": "https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/releases/download/home-shared-83afd19e5184eb511cc205299c83eb9cb5c05430/product-x4-store-store.bin",
    "sha256": "85a20e1d31b9cbd8a668b2a0032de7644f3796d868ea966fe2a55846aa285bfe",
    "size": 5308416
  }
}
def checked_download(name,root):
 row=BASELINE[name];path=root/name
 if not path.exists():
  with urllib.request.urlopen(row['url'],timeout=180) as response:path.write_bytes(response.read())
 data=path.read_bytes()
 if len(data)!=row['size'] or digest(data)!=row['sha256']:raise ValueError('Published baseline bytes differ '+name)
 return data
def read_zip(data):
 with zipfile.ZipFile(io.BytesIO(data)) as z:
  names=z.namelist()
  if len(names)!=len(set(names)):raise ValueError('Duplicate SD path')
  return {safe(n):z.read(n) for n in names if not n.endswith('/')}
def baseline(tools,output):
 output.mkdir(parents=True,exist_ok=False);downloads=output/'downloads';downloads.mkdir()
 sys.path.insert(0,str(tools.resolve()))
 from read_only_spiffs import read_image
 old_assembly=json.loads(checked_download('assembly.json',downloads))
 products={};inspection={}
 for lane in ('watch','wave','x4'):
  raw=checked_download('product-'+lane+'-store-store.bin',downloads)
  stored=read_image(raw,len(raw));before={n:decode_image(b) if b.startswith(MAGIC) else b for n,b in stored.items()}
  sd=read_zip(checked_download('product-'+lane+'-sd-sd-apps.zip',downloads))
  encoding=old_assembly['products'][lane].get('image_storage')
  if encoding:
   if {n:digest(b) for n,b in before.items()}!=encoding['canonical_files'] or {n:digest(b) for n,b in stored.items()}!=encoding['stored_files']:raise ValueError('Published envelope inventory differs')
  boot=json.loads(before['boot.json'])
  app_rows=[r for r in boot['app_capabilities'] if json.loads(before[r['manifest']]).get('file_name')=='ble_touchpad.elf']
  scene_rows=[r for r in boot['drivers'] if any(c['capability']=='ui.scene' for c in json.loads(before[r['manifest']]).get('provides',[]))]
  if len(app_rows)!=1 or len(scene_rows)!=1:raise ValueError('Touchpad/scene authority is not unique '+lane)
  inspection[lane]={'store_size':len(raw),'payload_size':sum(map(len,stored.values())),'app_row':app_rows[0],'app_manifest':json.loads(before[app_rows[0]['manifest']]),'scene_row':scene_rows[0],'scene_manifest':json.loads(before[scene_rows[0]['manifest']]),'internal_touchpad':[(n,len(b),digest(b)) for n,b in before.items() if 'touchpad' in n],'sd_touchpad':[(n,len(b),digest(b)) for n,b in sd.items() if 'touchpad' in n],'providers':len(boot['drivers']),'applications':len(boot['app_capabilities']),'native_metadata':json.loads(before['cohort.json']) if 'cohort.json' in before else None}
  write_tree(output/lane/'before',before);write_tree(output/lane/'before-stored',stored);write_tree(output/lane/'before-sd',sd)
  products[lane]=(before,stored,sd,len(raw))
 (output/'inspection.json').write_bytes(encode(inspection))
 print(json.dumps(inspection,sort_keys=True,indent=2),flush=True)
 return products
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--bootfs-tools',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--inspect-only',action='store_true');a=p.parse_args()
 baseline(a.bootfs_tools,a.output)

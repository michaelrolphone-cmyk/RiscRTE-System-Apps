#!/usr/bin/env python3
"""Cumulative USB overlay: preserve all non-USB canonical AND stored bytes."""
import argparse, copy, json, sys, zipfile
from pathlib import Path
from compose_shared_app_cohort import tree, write_tree, encode, digest, safe
from assemble_shared_home_cohort import inventory, fingerprint, pack_candidate, providers
from image_envelope import MAGIC, decode_image, encode_image, receipt as image_receipt
from image_storage import ordinary_images
ROOT=Path(__file__).resolve().parents[1]
LANES=('watch','wave','x4')
MANIFEST='usb_sd_transfer.json'
IMAGE='usb_sd_transfer.elf'
SD_IMAGE='Apps/usb_sd_transfer/usb_sd_transfer.elf'
def grant(cap,api=1,instance=0):
 return {'capability':cap,'api':api,'instance_id':instance}
OLD={
 'wave':{'manifest':MANIFEST,'image_source':{'path':SD_IMAGE,'volume_instance':12},
         'grants':[grant('display.output',instance=5),grant('input.touch.raw',instance=6),grant('usb.device.msc')]},
 'x4':{'manifest':MANIFEST,'grants':[grant('display.output',instance=3),grant('input.touch.raw',instance=4),
        grant('input.navigation',instance=6),grant('usb.device.msc'),grant('alarm.service',2)]}}
def decode_store(stored):
 return {n:decode_image(b) if b.startswith(MAGIC) else b for n,b in stored.items()}
def check_common(raw):
 m=json.loads(raw)
 expected={'type':'application','id':'usb_sd_transfer','architecture':'xtensa-esp32s3',
           'file_name':IMAGE,'entry':'app_main'}
 if any(m.get(k)!=v for k,v in expected.items()):raise ValueError('Invalid common application identity')
 if m.get('requires')!=[{'capability':'ui.scene','api':1}]:raise ValueError('Common scene contract differs')
 if sorted(m.get('optional',[]),key=lambda x:x['capability'])!=[
    {'capability':'alarm.service','api':2},{'capability':'usb.device.msc','api':1}]:
  raise ValueError('Common optional MSC/alarm contract differs')
 return m
def overlay(lane,before,stored,sd,elf,manifest):
 if lane not in LANES:raise ValueError('Unknown lane')
 check_common(manifest)
 if not elf.startswith(b'\x7fELF'):raise ValueError('Expected canonical ELF')
 if decode_store(stored)!=before:raise ValueError('Stored baseline differs from canonical baseline')
 boot=json.loads(before['boot.json']);oldboot=copy.deepcopy(boot)
 available=providers(before,boot)
 if ('ui.scene',1,0) not in available:raise ValueError('Existing scene instance absent')
 rows=[r for r in boot['app_capabilities'] if r['manifest']==MANIFEST]
 out=dict(before);packed=dict(stored);new_sd=dict(sd)
 if lane=='watch':
  if rows or MANIFEST in before or IMAGE in before:raise ValueError('Watch already has USB')
  if any(cap=='usb.device.msc' for cap,api,instance in available):raise ValueError('Watch unexpectedly has MSC')
  row={'manifest':MANIFEST,'grants':[grant('ui.scene')]}
  boot['app_capabilities'].append(row)
 else:
  if rows!=[OLD[lane]]:raise ValueError('Pinned USB authority differs '+lane)
  old=json.loads(before[MANIFEST])
  if old.get('id')!='usb_sd_transfer' or old.get('file_name')!=IMAGE or old.get('type')!='application':
   raise ValueError('Existing USB manifest identity differs')
  row=copy.deepcopy(rows[0])
  row['grants']=[grant('ui.scene'),grant('usb.device.msc')]
  if lane=='x4':row['grants'].append(grant('alarm.service',2))
  boot['app_capabilities'][boot['app_capabilities'].index(rows[0])]=row
 for g in row['grants']:
  if (g['capability'],g['api'],g['instance_id']) not in available:
   raise ValueError('Existing provider instance absent '+repr(g))
 # The only policy change is replacement/addition of the USB app row.
 if {k:v for k,v in oldboot.items() if k!='app_capabilities'}!={k:v for k,v in boot.items() if k!='app_capabilities'}:
  raise ValueError('Unrelated boot field changed')
 if [r for r in oldboot['app_capabilities'] if r['manifest']!=MANIFEST]!=[r for r in boot['app_capabilities'] if r['manifest']!=MANIFEST]:
  raise ValueError('Unrelated app authority changed')
 out['boot.json']=encode(boot);out[MANIFEST]=manifest
 packed['boot.json']=out['boot.json'];packed[MANIFEST]=manifest
 if lane=='wave':
  if SD_IMAGE not in sd:raise ValueError('Installed Wave SD executable absent')
  new_sd[SD_IMAGE]=elf
 else:
  if lane=='x4' and IMAGE not in before:raise ValueError('Installed X4 executable absent')
  out[IMAGE]=elf;packed[IMAGE]=elf
 allowed={'boot.json',MANIFEST}|({IMAGE} if lane!='wave' else set())
 if set(out)-set(before)!=({MANIFEST,IMAGE} if lane=='watch' else set()):raise ValueError('Unexpected additions')
 if set(before)-set(out) or any(out[n]!=b for n,b in before.items() if n not in allowed):raise ValueError('Unrelated canonical bytes changed')
 if set(stored)-set(packed) or any(packed[n]!=b for n,b in stored.items() if n not in allowed):raise ValueError('Unrelated stored bytes changed')
 if set(new_sd)!=set(sd) or any(new_sd[n]!=b for n,b in sd.items() if not(lane=='wave' and n==SD_IMAGE)):
  raise ValueError('Unrelated SD bytes changed')
 return out,packed,new_sd
def storage_record(canonical,stored):
 if decode_store(stored)!=canonical:raise ValueError('Canonical recovery differs')
 encoded={n:b for n,b in stored.items() if b.startswith(MAGIC)}
 if not encoded:return None
 if not set(encoded)<=ordinary_images(canonical):raise ValueError('Encoded non-ordinary app')
 return {'schema':1,'format':'RTEIMG1','images':{n:image_receipt(canonical[n],b) for n,b in encoded.items()},
         'canonical_files':{n:digest(b) for n,b in sorted(canonical.items())},
         'stored_files':{n:digest(b) for n,b in sorted(stored.items())},
         'canonical_recovery_verified':True,'selection':'Preserve baseline encoding; encode only added USB if required'}
def run(baseline,application,tools,output):
 if output.exists():raise ValueError('Output exists')
 sys.path.insert(0,str(tools.resolve()))
 from current_bootfs import build
 from read_only_spiffs import read_image
 lock=json.loads((ROOT/'docs/shared-usb/baseline.json').read_text())
 for row in lock['assets']:
  data=(baseline/row['name']).read_bytes()
  if len(data)!=row['bytes'] or digest(data)!=row['sha256']:raise ValueError('Public baseline asset mismatch')
 elf=(application/IMAGE).read_bytes();manifest=(application/MANIFEST).read_bytes()
 built=json.loads((application/'build.json').read_text())
 if built['sha256']!=digest(elf) or built['bytes']!=len(elf) or built['manifest_sha256']!=digest(manifest):
  raise ValueError('Canonical build identity differs')
 if set(built.get('exports',[]))!={'app_main','app_module_init','app_module_fini','risc_resident_app_descriptor_v1'}:
  raise ValueError('Shared foreground resident descriptor missing')
 results={};output.mkdir(parents=True)
 for lane in LANES:
  raw=(baseline/f'product-{lane}-store-store.bin').read_bytes()
  stored=read_image(raw,len(raw));before=decode_store(stored)
  with zipfile.ZipFile(baseline/f'product-{lane}-sd-sd-apps.zip') as z:
   names=z.namelist()
   if len(names)!=len(set(names)):raise ValueError('Duplicate baseline SD member')
   sd={safe(n):z.read(n) for n in names}
  after,candidate,after_sd=overlay(lane,before,stored,sd,elf,manifest)
  try:build(candidate,len(raw))
  except Exception as error:
   if lane!='watch' or type(error).__name__!='SpiffsFullError':raise
   # Runtime 0.4.20 baseline already carries the qualified decoder. Do not
   # re-encode Home or providers, nor widen the eligible transformation set.
   if not any(b.startswith(MAGIC) for b in stored.values()):raise ValueError('Expected encoded Home baseline')
   encoded=encode_image(elf)
   if len(encoded)>=len(elf):raise ValueError('USB encoding cannot improve fit')
   candidate[IMAGE]=encoded
  encoding=storage_record(after,candidate)
  path=output/lane;path.mkdir()
  for name,entries in [('before',before),('before-stored',stored),('before-sd',sd),
                       ('after',after),('stored',candidate),('after-sd',after_sd)]:
   (path/name).mkdir();write_tree(path/name,entries)
  result={'inventory_sha256':fingerprint(inventory(before,sd)),
          'shared_elf_sha256':digest(elf),'manifest_sha256':digest(manifest),
          'baseline_store_sha256':digest(raw),'image_storage':encoding,
          'changed_internal':[n for n in sorted(after) if after[n]!=before.get(n)],
          'changed_stored':[n for n in sorted(candidate) if candidate[n]!=stored.get(n)],
          'changed_sd':[n for n in sorted(after_sd) if after_sd[n]!=sd.get(n)],
          'unrelated_canonical_bytes_preserved':True,'unrelated_stored_bytes_preserved':True,
          'unrelated_sd_bytes_preserved':True,'provider_and_data_ownership_preserved':True}
  result.update(pack_candidate(path,candidate,after_sd,len(raw),build,read_image))
  results[lane]=result
  print('USB PRODUCT '+lane+' '+json.dumps(result),flush=True)
 (output/'assembly.json').write_bytes(encode({'schema':1,'products':results,'packed':True,
  'shared_elf_sha256':digest(elf),'manifest_sha256':digest(manifest),
  'baseline_tag':lock['tag'],'physical_testing':False}))
 return results
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('baseline','application','bootfs-tools','output'):p.add_argument('--'+name,type=Path,required=True)
 a=p.parse_args();run(a.baseline,a.application,a.bootfs_tools,a.output)

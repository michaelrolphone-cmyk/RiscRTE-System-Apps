#!/usr/bin/env python3
"""Offline Files product projection. Stream input hashes; preserve every unrelated byte and authority row."""
import argparse,copy,hashlib,io,json,sys,zipfile
from pathlib import Path,PurePosixPath
from files_package_identity import check_identity
p=argparse.ArgumentParser(description=__doc__)
for k in ('inputs','selections','home-packages','home-pins','bootfs-tools','output'):p.add_argument('--'+k,type=Path,required=True)
p.add_argument('--lane',choices=['watch','wave','x4']);a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
def sha(b):return hashlib.sha256(b).hexdigest()
def file_sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def encode(v):return (json.dumps(v,sort_keys=True,indent=2)+'\n').encode()
def safe(n):
 q=PurePosixPath(n);assert n and not q.is_absolute() and '..' not in q.parts and '\\' not in n,n
 return n
expected={'watch.zip':'3a5aadb0eeb5c4b37640d606978162b5c0484893aada661baca4ca39a12d66f2','wave.zip':'aae8665d9240bd49dbdae89f250525ccea6cabc44215133248ee1a2c7b19d1d6','x4-store.bin':'8d38a77bdb3dd1439e8736d9c0363798f20803e02b44fc1aa5dab578207c4adc','x4-sd.zip':'2b6dce3efb468f3cd728b50707e0c9552d84e225b65e2b27e683d3d726bc6cef','watchrtc.zip':'4ab38e133604991f2d09a2739f73dccc5dfa413e94527676795df9667378a671','wavertc.zip':'80fa5c1805f21eb97ee844a495bcde1e00c4c53d5eecf89ac204d64dbbaefa1b'}
for n,h in expected.items():
 if a.lane and n not in {'watch':['watch.zip','watchrtc.zip'],'wave':['wave.zip','wavertc.zip'],'x4':['x4-store.bin','x4-sd.zip']}[a.lane]:continue
 assert file_sha(a.inputs/n)==h,n
for n,h in {'current_bootfs.py':'6f0997a71fe6d2abbb97ed91e0e4180abf43b2056afbcb5b54d303437791576b','read_only_spiffs.py':'e48d3b2422c60f81fdae6371ff3c5b65c5cbaa9dd250b4a2925c74c0b45f040d'}.items():assert file_sha(a.bootfs_tools/n)==h,n
sys.path.insert(0,str(a.bootfs_tools));from read_only_spiffs import read_image
from current_bootfs import build
selection=json.loads((a.selections/'receipt.json').read_text());pins=json.loads(a.home_pins.read_text());results={}
def checked_zip(p):
 z=zipfile.ZipFile(p);names=z.namelist();assert len(names)==len(set(names))
 for n in names:safe(n)
 return z
def home_pair(lane,baseline):
 first=a.home_packages/lane/'first';repeat=a.home_packages/lane/'repeat';b=json.loads((first/'build.json').read_text());r=json.loads((repeat/'build.json').read_text())
 for name in ('default.elf','default.json'):assert (first/name).read_bytes()==(repeat/name).read_bytes()
 for rec in (b,r):
  assert rec['compiler_sha256']==pins['compiler_sha256'] and rec['files_effects_source_pins_sha256']==file_sha(a.home_pins)
  assert rec['automatic_provision'] and bool(rec.get('serial_transfer_mode'))==(lane=='wave')
  assert rec['home_files_sha256']['apps/clock/clock_resident_effects.inc']==pins['sources']['home']['files']['apps/clock/clock_resident_effects.inc']
  for role in ('system','utilities'):
   dep=rec['verified_dependencies'][role];assert dep['base_commit']==pins['sources'][role]['base_commit']
   for n,h in dep['files_sha256'].items():assert pins['sources'][role]['files'][n]==h
  for n,h in rec['home_files_sha256'].items():assert pins['sources']['home']['files'][n]==h
  assert rec['artifacts']['default']['sha256']==file_sha(first/'default.elf')
 for k in ('home_files_sha256','verified_dependencies','compiler_sha256','artifacts'):assert b[k]==r[k]
 new=json.loads((first/'default.json').read_text());old=json.loads(baseline)
 assert new['version']==('0.26.1' if lane=='watch' else '0.26.2')
 assert {k:v for k,v in new.items() if k!='version'}=={k:v for k,v in old.items() if k!='version'}
 return (first/'default.elf').read_bytes(),encode(new),{'first_build_sha256':file_sha(first/'build.json'),'repeat_build_sha256':file_sha(repeat/'build.json'),'elf_sha256':file_sha(first/'default.elf'),'repeat_identical':True}
for lane in ([a.lane] if a.lane else ['watch','wave','x4']):
 d=out/lane;d.mkdir();selected=a.selections/lane;elf=(selected/'file_browser.elf').read_bytes();manifest=(selected/'file_browser.json').read_bytes();sel=selection['selections'][lane];assert sha(elf)==sel['elf_sha256']
 original=json.loads((selected/'original-policy-rows.json').read_text());proposed=json.loads((selected/'proposed-policy-rows.json').read_text())
 for old,new in zip(original,proposed,strict=True):
  q=copy.deepcopy(new);assert q['grants'].pop()=={'capability':'ui.scene','api':1,'instance_id':0};assert q==old
 if lane=='x4':raw=(a.inputs/'x4-store.bin').read_bytes();sdsource=a.inputs/'x4-sd.zip'
 else:
  with checked_zip(a.inputs/(lane+'.zip')) as z:
   raw=z.read('candidate/store.bin');sdsource=io.BytesIO(z.read('candidate/sd-apps.zip')) if lane=='wave' else None
 before=read_image(raw,len(raw));after=dict(before);boot=json.loads(before['boot.json']);assert sha(before['boot.json'])==sel['original_boot_sha256']
 rows=[x for x in boot['app_capabilities'] if x.get('manifest')=='file_browser.json'];assert rows==original
 nextboot=copy.deepcopy(boot)
 for i,row in enumerate(nextboot['app_capabilities']):
  if row.get('manifest')=='file_browser.json':nextboot['app_capabilities'][i]=copy.deepcopy(proposed[rows.index(row)])
 row=rows[0];route=row.get('image_source',{}).get('path');assert not route or '/image-' not in route
 if 'file_browser.elf' in after:after['file_browser.elf']=elf
 after['file_browser.json']=manifest;home=None
 if lane!='x4':
  after['default.elf'],after['default.json'],home=home_pair(lane,before['default.json'])
  with checked_zip(a.inputs/(lane+'rtc.zip')) as z:
   rtc=z.read('rtc-utc/package/driver.elf');assert rtc==z.read('rtc-utc/repeat/driver.elf');m=json.loads(z.read('rtc-utc/package/manifest.json'));old=json.loads(before['rtc/manifest.json']);assert {k:v for k,v in m.items() if k!='version'}=={k:v for k,v in old.items() if k!='version'}
   receipt=json.loads(z.read('rtc-utc/receipt.json'));assert '-DRTC_UTC_OFFSET_SECONDS=28800' in receipt['builds'][0]['defines'] and sha(rtc)==receipt['builds'][0]['sha256']
  after['rtc/driver.elf']=rtc;after['rtc/manifest.json']=encode(m)
 identities=[]
 old_file=before.get('file_browser.elf')
 if old_file is None:
  assert sdsource and route
  with checked_zip(sdsource) as z:old_file=z.read(route)
 identities.append(check_identity('Files',before['file_browser.json'],manifest,old_file,elf))
 if lane!='x4':
  for name,mp,ep in [('Home','default.json','default.elf'),('RTC','rtc/manifest.json','rtc/driver.elf')]:
   identities.append(check_identity(name,before[mp],after[mp],before[ep],after[ep]))
 after['boot.json']=encode(nextboot)
 allowed={'boot.json','file_browser.elf','file_browser.json'}|({'default.elf','default.json','rtc/driver.elf','rtc/manifest.json'} if lane!='x4' else set())
 assert before.keys()==after.keys();changed={n for n in before if before[n]!=after[n]};assert changed<=allowed
 assert nextboot['drivers']==boot['drivers'];assert [r for r in nextboot['app_capabilities'] if r.get('manifest')!='file_browser.json']==[r for r in boot['app_capabilities'] if r.get('manifest')!='file_browser.json']
 print(json.dumps({'lane':lane,'store_bytes':len(raw),'before_payload_bytes':sum(map(len,before.values())),'after_payload_bytes':sum(map(len,after.values())),'changed_sizes':{n:{'before':len(before[n]),'after':len(after[n])} for n in sorted(changed)}}),flush=True)
 image,packing=build(after,len(raw));assert read_image(image,len(raw))==after;(d/'store.bin').write_bytes(image)
 for which,entries in [('before',before),('after',after)]:
  for n,b in entries.items():f=d/which/safe(n);f.parent.mkdir(parents=True,exist_ok=True);f.write_bytes(b)
 sdchanges=[];sdcount=0
 if sdsource:
  with checked_zip(sdsource) as src,zipfile.ZipFile(d/'sd-apps.zip','w',compression=zipfile.ZIP_DEFLATED) as dest:
   for entry in src.infolist():
    if entry.is_dir():continue
    n=safe(entry.filename);b=src.read(entry);nextbytes=elf if n==route else b
    dest.writestr(n,nextbytes);sdcount+=1
    if nextbytes!=b:sdchanges.append(n)
   assert sdchanges==([route] if route else [])
  with checked_zip(sdsource) as src,checked_zip(d/'sd-apps.zip') as dest:
   assert {n for n in src.namelist() if not n.endswith('/')}==set(dest.namelist())
   for n in dest.namelist():assert dest.read(n)==(elf if n==route else src.read(n))
 results[lane]={'package_identities':identities,'app_sha256':sha(elf),'app_bytes':len(elf),'changed_internal':sorted(changed),'sd_changed':sdchanges,'sd_members':sdcount,'home':home,'packing':packing,'authority_delta':'ui.scene API1 instance0 only','namespace_maps_and_other_rows_unchanged':True,'data_files_unchanged':True,'target_execution':False}
 del before,after,raw,image
 (out/'assembly.json').write_bytes(encode(results));print(lane,'preservation and store roundtrip PASS',flush=True)
assert len({v['app_sha256'] for v in results.values()})==1

#!/usr/bin/env python3
"""Offline tests of the actual publication validator; never calls GitHub."""
import hashlib, importlib.util, json, os, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('publisher',ROOT/'scripts/publish_home_shared.py')
publisher=importlib.util.module_from_spec(spec);spec.loader.exec_module(publisher)
COMMIT='a'*40
os.environ.update(GITHUB_REPOSITORY='michaelrolphone-cmyk/RiscRTE-System-Apps',GITHUB_SHA=COMMIT)
os.environ.pop('GITHUB_TOKEN',None)
def put(root,name,value):
 data=value if isinstance(value,bytes) else (json.dumps(value,sort_keys=True)+'\n').encode()
 path=root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
 return {'path':name,'sha256':hashlib.sha256(data).hexdigest(),'size':len(data)}
def make(root,fault):
 encoded=fault.startswith('encoded-')
 elf=put(root,'target/default.elf',b'\x7fELFfixture-not-a-target-proof')
 manifest=put(root,'target/default.json',{'name':'home'})
 packed_store=put(root,'store.bin',b'fixture store');packed_sd=put(root,'sd.zip',b'fixture sd')
 native_refs={};native_bindings={}
 for lane in ('watch','wave','x4'):
  firmware=put(root,lane+'/firmware.bin',b'fixture firmware');qualification=put(root,lane+'/qualification.json',{'fixture':True})
  native={key:{'path':Path(value['path']).name,'bytes':value['size'],'sha256':value['sha256']} for key,value in [('firmware',firmware),('qualification',qualification)]}
  if encoded:native.update(runtime_revision='712ecc3f894dba99d9c9638941778a82f5856b97',runtime_version='0.4.20')
  if fault=='encoded-wrong-runtime' and lane=='watch':native['runtime_version']='0.4.19'
  descriptor=put(root,lane+'/native.json',native)
  native_refs[lane]={'firmware':firmware,'qualification':qualification,'descriptor':descriptor}
  native_bindings[lane]={'native':native,'descriptor_sha256':descriptor['sha256'],'after_sha256':'6'*64}
 assembly={'packed':True,'byte_identical_home':elf['sha256'],'products':{}}
 for lane in ('watch','wave','x4'):
  assembly['products'][lane]={'home_sha256':elf['sha256'],'packed':True,'store_roundtrip':True,'sd_roundtrip':True,
    'store_sha256':packed_store['sha256'],'sd_archive_sha256':packed_sd['sha256'],'inventory_sha256':'3'*64,'native_metadata':native_bindings[lane]}
 if encoded:
  mapping={'app.elf':'4'*64,'cohort.json':'6'*64};stored={**mapping,'app.elf':'a'*64}
  assembly['products']['watch']['image_storage']={'format':'RTEIMG1','canonical_recovery_verified':True,'canonical_files':mapping,'stored_files':stored,'images':{'app.elf':{'canonical_sha256':'4'*64,'stored_sha256':'a'*64,'canonical_bytes':4096,'stored_bytes':512,'codec':'lz4-block','header_bytes':32,'roundtrip_verified':True}}}
  if fault=='encoded-no-recovery':assembly['products']['watch']['image_storage']['canonical_recovery_verified']=False
  if fault=='encoded-wrong-codec':assembly['products']['watch']['image_storage']['images']['app.elf']['codec']='other'
 if fault=='unpacked': assembly['packed']=False
 if fault=='different-elf':assembly['products']['wave']['home_sha256']='b'*64
 assembly_ref=put(root,'assembly.json',assembly)
 delivery={'source_commit':COMMIT,'application':{'elf':elf,'manifest':manifest,'build':put(root,'target/build.json',{'shared_elf':elf['sha256'],'manifest_sha256':manifest['sha256'],'size_bytes':elf['size'],'views':['native-faces','overview-retained-desk']})},
  'assembly':assembly_ref,'admissions':{},'dependencies':[],'native_firmware':native_refs}
 delivery['product_assets']={lane:{'store':packed_store,'sd':packed_sd} for lane in ('watch','wave','x4')}
 if fault=='wrong-native':delivery['native_firmware']['wave']['firmware']=put(root,'wrong-native.bin',b'wrong firmware')
 if fault=='missing-native':del delivery['native_firmware']['x4']
 if fault=='wrong-product':delivery['product_assets']['wave']['store']=put(root,'wrong-store.bin',b'wrong')
 for mode in ('normal','sanitized'):
  binding={'source_commit':COMMIT,'assembly_sha256':assembly_ref['sha256'],'products':{}}
  receipt={'schema':1,'sanitized':mode=='sanitized','products':{},'product_binding':binding,'candidate_input_files':{}}
  for lane in ('watch','wave','x4'):
   mapping={'app.elf':'4'*64,'cohort.json':'6'*64}
   tree_hash=hashlib.sha256((json.dumps(mapping,sort_keys=True,separators=(',',':'))+'\n').encode()).hexdigest()
   receipt['candidate_input_files'][lane]={which:dict(mapping) for which in ('before','after','stored','after-sd')}
   binding['products'][lane]={'inventory_sha256':'3'*64,'before_tree_sha256':tree_hash,'after_tree_sha256':tree_hash,'stored_tree_sha256':tree_hash,'after_sd_tree_sha256':tree_hash}
   receipt['products'][lane]={'inputs':{'fixture':'7'*64},'per_tu_dependencies':{'fixture':'8'*64},'results':[{'input':'after','result':{'prepared':True,'cohort_validated':True,'elf_count':1,'hardware_calls':0,'storage_calls':0,'retained_sd_serial_graph_bound':True}}]}
  if encoded:
   receipt['candidate_input_files']['watch']['stored']={'app.elf':'a'*64,'cohort.json':'6'*64}
   binding['products']['watch']['stored_tree_sha256']=hashlib.sha256((json.dumps(receipt['candidate_input_files']['watch']['stored'],sort_keys=True,separators=(',',':'))+'\n').encode()).hexdigest()
   receipt['products']['watch']['results'][0]['result']['encoded_image_count']=0 if fault=='encoded-no-reader' else 1
  if fault=='unrecorded-stored-change':receipt['candidate_input_files']['watch']['stored']['app.elf']='a'*64
  if fault=='missing-stored':del receipt['candidate_input_files']['watch']['stored']
  if fault=='unrecorded-decoding':receipt['products']['watch']['results'][0]['result']['encoded_image_count']=1
  if fault=='wrong-native-cohort':receipt['candidate_input_files']['wave']['after']['cohort.json']='7'*64
  if fault=='prepare-only':receipt['prepare_only']=True
  if fault=='null-binding':receipt['product_binding']=None
  if fault=='changed-input-map':receipt['candidate_input_files']['watch']['after']['app.elf']='5'*64
  if fault=='failed-admission':receipt['products']['x4']['results'][0]['result']['cohort_validated']=False
  if fault=='wrong-tree':binding['products']['wave']['inventory_sha256']='9'*64
  if fault=='wrong-assembly':binding['assembly_sha256']='9'*64
  if fault=='hardware':receipt['products']['watch']['results'][0]['result']['hardware_calls']=1
  delivery['admissions'][mode]=put(root,'admission-'+mode+'.json',receipt)
 if fault=='missing-sanitized':del delivery['admissions']['sanitized']
 dependency=put(root,'dependency.bin',b'dependency')
 delivery['dependencies']=[{'receipt':put(root,'dependency.json',{'owning':'receipt'}),'artifacts':[dependency],'sources':[dependency],'licenses':[dependency]}]
 for field in ('source_archive','provenance','licenses','qualification'):
  delivery[field]=put(root,field+'.bin',b'fixture')
 delivery['source_lock']=put(root,'source-lock.json',{'commits':{'system_apps':COMMIT}})
 if fault=='wrong-build':
  delivery['application']['build']=put(root,'target/build.json',{'shared_elf':'b'*64,'manifest_sha256':manifest['sha256'],'size_bytes':elf['size'],'views':['native-faces','overview-retained-desk']})
 if fault=='missing-view':
  delivery['application']['build']=put(root,'target/build.json',{'shared_elf':elf['sha256'],'manifest_sha256':manifest['sha256'],'size_bytes':elf['size'],'views':['native-faces']})
 if fault=='wrong-source':delivery['source_commit']='b'*40
 if fault=='traversal':delivery['application']['elf']['path']='../escape'
 (root/'home-delivery.json').write_text(json.dumps(delivery))
 if fault=='tampered':(root/'target/default.elf').write_bytes(b'changed')
for fault in ('good','encoded-good','encoded-wrong-runtime','encoded-no-recovery','encoded-wrong-codec','encoded-no-reader','unrecorded-stored-change','missing-stored','unrecorded-decoding','wrong-native','missing-native','wrong-native-cohort','wrong-product','changed-input-map','null-binding','wrong-build','missing-view','unpacked','different-elf','prepare-only','failed-admission','wrong-tree','wrong-assembly','hardware','missing-sanitized','wrong-source','traversal','tampered'):
 with tempfile.TemporaryDirectory() as temporary:
  root=Path(temporary);make(root,fault)
  try:publisher.run(root,False)
  except (ValueError,TypeError,KeyError):
   assert fault not in ('good','encoded-good'),fault
  else:
   assert fault in ('good','encoded-good'),fault
   plan=json.loads((root/'publication-plan.json').read_text())['assets']
   for category in ('artifacts','sources','licenses'):assert any(name.startswith('dependency-0-'+category+'-') for name in plan)
   for lane in ('watch','wave','x4'):
    for category in ('store','sd'):assert any(name.startswith('product-'+lane+'-'+category+'-') for name in plan)
print('Home publisher offline validator: raw and encoded complete fixtures accepted; partial/mismatched/unsafe closures rejected; no network or publication')

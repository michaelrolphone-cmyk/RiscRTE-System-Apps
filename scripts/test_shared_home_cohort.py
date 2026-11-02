#!/usr/bin/env python3
"""Synthetic overlay contract regressions; not product binding evidence."""
import copy,json,tempfile,unittest
from pathlib import Path
from assemble_shared_home_cohort import inventory,fingerprint,overlay,pack_candidate,replace_native_metadata,validate_bootfs_names,payload_inventory,validate_grant_substitutions
from compose_shared_app_cohort import encode,digest
class Overlay(unittest.TestCase):
 def fixture(self):
  row={'manifest':'default.json','grants':[{'capability':'storage.key-value','api':1,'namespace':5}]}
  boot={'drivers':[],'app_capabilities':[row,{'manifest':'clock.json','grants':[]}],'namespace_map':{'points':5}}
  before={'boot.json':encode(boot),'default.json':encode({'type':'application','id':'installed-home','label':'My Home','file_name':'default.elf','version':'old'}),
          'default.elf':b'old','clock.json':encode({'type':'application','id':'twatch-clock-return','file_name':'clock.elf'}),
          'clock.elf':b'return-trampoline','saved/points':b'PTC1 unchanged'}
  sd={'books/read.epub':b'original book'}
  alias={'role':'controller','manifest':'default.json','route':{'storage':'internal','path':'default.elf'},
         'original_policy_rows':[copy.deepcopy(row)],'policy_rows':[copy.deepcopy(row)]}
  plan={'inventory_sha256':fingerprint(inventory(before,sd)),'aliases':[alias],'providers':[]}
  common={'type':'application','entry':'app_main','architecture':'xtensa-esp32s3','version':'1.0.0','requires':[],'optional':[]}
  return before,sd,plan,common
 def apply(self,b,sd,p,m):
  return overlay(b,sd,p,b'\x7fELFcomplete-dual-view',m,Path('.'))
 def test_preserve(self):
  b,sd,p,m=self.fixture();a,s,r=self.apply(b,sd,p,m)
  self.assertEqual(s,sd);self.assertEqual(a['clock.elf'],b['clock.elf']);self.assertEqual(a['clock.json'],b['clock.json'])
  self.assertEqual(a['saved/points'],b['saved/points'])
  self.assertEqual(json.loads(a['default.json'])['id'],'installed-home')
  self.assertEqual(json.loads(a['default.json'])['label'],'My Home')
  self.assertEqual(json.loads(a['boot.json']),json.loads(b['boot.json']))
  self.assertFalse(r['packed'])
 def test_inventory_lease(self):
  b,sd,p,m=self.fixture();b['saved/points']=b'new user data'
  with self.assertRaises(ValueError):self.apply(b,sd,p,m)
 def test_no_removed_grant(self):
  b,sd,p,m=self.fixture();p['aliases'][0]['policy_rows'][0]['grants']=[]
  with self.assertRaises(ValueError):self.apply(b,sd,p,m)
 def test_no_namespace_rewrite(self):
  b,sd,p,m=self.fixture();p['aliases'][0]['policy_rows'][0]['namespace_map']={'points':6}
  with self.assertRaises(ValueError):self.apply(b,sd,p,m)
 def test_route_refusal(self):
  b,sd,p,m=self.fixture();p['aliases'][0]['route']['path']='clock.elf'
  with self.assertRaises(ValueError):self.apply(b,sd,p,m)
 def test_all_boot_manifests(self):
  b,sd,p,m=self.fixture();self.assertEqual(set(inventory(b,sd)['manifests']),{'default.json','clock.json'})
 def test_deployment_metadata_inventory(self):
  b,sd,p,m=self.fixture()
  value={'firmware_size':123,'firmware_sha256':'original','runtime_version':'old','extra':{'preserve':True}}
  b['cohort.json']=encode(value)
  record=inventory(b,sd)
  self.assertEqual(record['deployment_metadata'],{'cohort.json':value})
  self.assertEqual(record['files']['cohort.json'],digest(b['cohort.json']))
 def native_fixture(self,root):
  pkg=root/'native';pkg.mkdir()
  firmware=b'firmware:RISC_RUNTIME_VERSION:0.4.19\0'
  qualification=encode({'owner':'synthetic test only'})
  (pkg/'firmware.bin').write_bytes(firmware);(pkg/'qualification.json').write_bytes(qualification)
  native={'source_revision':'a'*40,'runtime_revision':'b'*40,'runtime_version':'0.4.19',
          'firmware':{'path':'firmware.bin','bytes':len(firmware),'sha256':digest(firmware)},
          'qualification':{'path':'qualification.json','bytes':len(qualification),'sha256':digest(qualification)},
          'artifact':{'repository':'fixture','id':1,'sha256':'c'*64}}
  raw=encode(native);(pkg/'native.json').write_bytes(raw)
  old={'product':'preserved-product','version':'old','runtime_version':'old-runtime',
       'firmware_size':1,'firmware_sha256':'old-hash','unrelated':{'data_namespace':5}}
  entries={'cohort.json':encode(old)}
  spec={'path':'cohort.json','before_sha256':digest(entries['cohort.json']),'package':'native',
        'descriptor_sha256':digest(raw),'product_version':'new',
        'fields':{k:{'path':[k],'before':old[k]} for k in ('version','runtime_version','firmware_size','firmware_sha256')}}
  return entries,spec,native
 def test_native_metadata_exact_bound_preservation(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);entries,spec,native=self.native_fixture(root)
   receipt=replace_native_metadata(entries,spec,root);out=json.loads(entries['cohort.json'])
   self.assertEqual(out['product'],'preserved-product');self.assertEqual(out['unrelated'],{'data_namespace':5})
   self.assertEqual(out['firmware_sha256'],native['firmware']['sha256'])
   self.assertEqual(out['firmware_size'],native['firmware']['bytes'])
   self.assertEqual(out['version'],'new');self.assertEqual(receipt['native'],native)
 def test_observed_native_cohort_mapping(self):
  from prepare_home_cohort_plan import native_metadata_plan,NATIVE_COHORT_BASELINE,CANDIDATE_PRODUCT_VERSIONS
  for lane in ('watch','wave','x4'):
   with self.subTest(lane=lane),tempfile.TemporaryDirectory() as tmp:
    root=Path(tmp);entries,spec,native=self.native_fixture(root)
    (root/'native').rename(root/('native-'+lane))
    native['artifact']['repository']=NATIVE_COHORT_BASELINE[lane]['source_repo']
    (root/('native-'+lane)/'native.json').write_bytes(encode(native))
    entries['cohort.json']=encode(NATIVE_COHORT_BASELINE[lane])
    plan=native_metadata_plan(entries,root,lane)
    replace_native_metadata(entries,plan,root);updated=json.loads(entries['cohort.json'])
    self.assertEqual(updated['version'],CANDIDATE_PRODUCT_VERSIONS[lane])
    self.assertEqual(updated['source_revision'],native['source_revision'])
    self.assertNotIn('runtime_revision',updated)
    for key in ('product','layout','store_abi','source_repo','schema','schema_version'):
     self.assertEqual(updated[key],NATIVE_COHORT_BASELINE[lane][key])
    with self.assertRaises(ValueError):native_metadata_plan(entries,root,lane)
 def test_native_metadata_refuses_stale_or_unbound_input(self):
  for fault in ('before','descriptor','firmware','qualification','field','missing','path','marker'):
   with self.subTest(fault=fault),tempfile.TemporaryDirectory() as tmp:
    root=Path(tmp);entries,spec,native=self.native_fixture(root)
    if fault=='before':spec['before_sha256']='stale'
    elif fault=='descriptor':spec['descriptor_sha256']='stale'
    elif fault in ('firmware','qualification'):(root/'native'/native[fault]['path']).write_bytes(b'changed')
    elif fault=='field':spec['fields']['version']['before']='stale'
    elif fault=='missing':del spec['fields']['firmware_size']
    elif fault=='path':spec['fields']['version']['path']=['absent']
    elif fault=='marker':
     native['runtime_version']='0.4.99';raw=encode(native);(root/'native/native.json').write_bytes(raw);spec['descriptor_sha256']=digest(raw)
    with self.assertRaises(ValueError):replace_native_metadata(entries,spec,root)
 def test_trampoline_refusal(self):
  b,sd,p,m=self.fixture();a=p['aliases'][0];a['manifest']='clock.json';a['route']['path']='clock.elf'
  a['original_policy_rows']=[{'manifest':'clock.json','grants':[]}];a['policy_rows']=copy.deepcopy(a['original_policy_rows'])
  with self.assertRaises(ValueError):self.apply(b,sd,p,m)
 def test_provider_collision(self):
  b,sd,p,m=self.fixture()
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);package=root/'raster';package.mkdir()
   manifest={'type':'driver','file_name':'driver.elf','provides':[{'capability':'ui.canvas.raster','api':1}]}
   raw=encode(manifest);elf=b'\x7fELFprovider'
   (package/'manifest.json').write_bytes(raw);(package/'driver.elf').write_bytes(elf)
   p['providers']=[{'package':'raster','manifest_sha256':digest(raw),'elf_sha256':digest(elf),'row':{'manifest':name+'/manifest.json','instance_id':0,'bindings':[]}} for name in ('one','two')]
   with self.assertRaises(ValueError):overlay(b,sd,p,b'\x7fELFhome',m,root)
 def test_independent_app_not_home_alias(self):
  b,sd,p,m=self.fixture()
  old={'type':'application','id':'contexts','file_name':'contexts.elf','label':'Contexts','version':'old'}
  row={'manifest':'contexts.json','grants':[{'capability':'ui.scene','api':1,'instance_id':0}]}
  boot=json.loads(b['boot.json']);boot['app_capabilities'].append(row);b['boot.json']=encode(boot)
  b['contexts.json']=encode(old);b['contexts.elf']=b'old-contexts'
  p['inventory_sha256']=fingerprint(inventory(b,sd))
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);pkg=root/'contexts';pkg.mkdir()
   new={**old,'entry':'app_main','version':'new','requires':[{'capability':'ui.scene','api':1}]}
   raw=encode(new);elf=b'\x7fELFcontexts'
   (pkg/'contexts.json').write_bytes(raw);(pkg/'contexts.elf').write_bytes(elf)
   p['app_updates']=[{'manifest':'contexts.json','package':'contexts','manifest_file':'contexts.json','elf_file':'contexts.elf',
     'manifest_sha256':digest(raw),'elf_sha256':digest(elf),'route':{'storage':'internal','path':'contexts.elf'},
     'original_policy_rows':[row],'policy_rows':[copy.deepcopy(row)],
     'replace':{'manifest_sha256':digest(b['contexts.json']),'elf_sha256':digest(b['contexts.elf'])}}]
   out,_,receipt=overlay(b,sd,p,b'\x7fELFhome',m,root)
   self.assertEqual(out['contexts.elf'],elf);self.assertEqual(out['default.elf'],b'\x7fELFhome')
   self.assertEqual(receipt['aliases'],['default.json'])
   self.assertEqual(receipt['application_updates'][0]['manifest'],'contexts.json')
   self.assertEqual(json.loads(out['contexts.json'])['label'],'Contexts')
 def test_exact_provider_replacement_preserves_authority(self):
  b,sd,p,m=self.fixture()
  old={'type':'driver','id':'scene-selected','file_name':'driver.elf','version':'0.4.14','provides':[{'capability':'ui.scene','api':1}],'requires':[]}
  new={**old,'version':'0.4.15'};row={'manifest':'scene/manifest.json','instance_id':0,'bindings':[]}
  boot=json.loads(b['boot.json']);boot['drivers']=[row];b['boot.json']=encode(boot)
  b['scene/manifest.json']=encode(old);b['scene/driver.elf']=b'\x7fELFold-selected-profile'
  p['inventory_sha256']=fingerprint(inventory(b,sd))
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);pkg=root/'selected';pkg.mkdir()
   raw=encode(new);elf=b'\x7fELFnew-selected-profile'
   (pkg/'manifest.json').write_bytes(raw);(pkg/'driver.elf').write_bytes(elf)
   item={'package':'selected','manifest_sha256':digest(raw),'elf_sha256':digest(elf),'row':row,
         'replace':{'manifest_sha256':digest(b['scene/manifest.json']),'elf_sha256':digest(b['scene/driver.elf'])}}
   p['providers']=[item]
   out,_,_=overlay(b,sd,p,b'\x7fELFhome',m,root)
   self.assertEqual(json.loads(out['boot.json'])['drivers'],[row])
   self.assertEqual(out['scene/driver.elf'],elf)
   item['replace']['elf_sha256']='wrong'
   with self.assertRaises(ValueError):overlay(b,sd,p,b'\x7fELFhome',m,root)
 def external_provider_fixture(self,root):
  b,sd,p,m=self.fixture()
  old={'type':'driver','id':'scene-selected','file_name':'driver.elf','version':'old',
       'provides':[{'capability':'ui.scene','api':1}],'requires':[]}
  row={'manifest':'scene/manifest.json','instance_id':0,'bindings':[],
       'key_value':[{'access':'read','key':'time_zone','namespace':1}],
       'image_source':{'volume_instance':9,
                       'path':'providers/scene.elf','internal_fallback':True}}
  boot=json.loads(b['boot.json']);boot['drivers']=[row];b['boot.json']=encode(boot)
  b['scene/manifest.json']=encode(old);b['scene/driver.elf']=b'\x7fELFold-fallback'
  sd['providers/scene.elf']=b'\x7fELFold-preferred'
  p['inventory_sha256']=fingerprint(inventory(b,sd))
  pkg=root/'selected';pkg.mkdir()
  raw=encode({**old,'version':'new'});elf=b'\x7fELFqualified'
  (pkg/'manifest.json').write_bytes(raw);(pkg/'driver.elf').write_bytes(elf)
  item={'package':'selected','manifest_sha256':digest(raw),'elf_sha256':digest(elf),'row':copy.deepcopy(row),
        'replace':{'manifest_sha256':digest(b['scene/manifest.json']),'elf_sha256':digest(b['scene/driver.elf']),
                   'external_route':{'path':'providers/scene.elf','elf_sha256':digest(sd['providers/scene.elf'])}}}
  p['providers']=[item]
  return b,sd,p,m,elf,row
 def test_external_provider_updates_preferred_and_fallback(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);b,sd,p,m,elf,row=self.external_provider_fixture(root)
   out,new_sd,receipt=overlay(b,sd,p,b'\x7fELFhome',m,root)
   self.assertEqual(out['scene/driver.elf'],elf)
   self.assertEqual(new_sd['providers/scene.elf'],elf)
   self.assertEqual(json.loads(out['boot.json'])['drivers'],[row])
   self.assertEqual(new_sd['books/read.epub'],sd['books/read.epub'])
   self.assertEqual(receipt['changed_sd'],['providers/scene.elf'])
   self.assertEqual(sd['providers/scene.elf'],b'\x7fELFold-preferred')
 def test_external_provider_refuses_unreviewed_routes(self):
  for fault in ('omitted','hash','path','missing','fallback','authority','internal-only','unsafe'):
   with self.subTest(fault=fault),tempfile.TemporaryDirectory() as tmp:
    root=Path(tmp);b,sd,p,m,elf,row=self.external_provider_fixture(root)
    item=p['providers'][0];replacement=item['replace']
    if fault=='omitted':del replacement['external_route']
    elif fault=='hash':replacement['external_route']['elf_sha256']='stale'
    elif fault=='path':replacement['external_route']['path']='books/read.epub'
    elif fault=='unsafe':replacement['external_route']['path']='../scene.elf'
    elif fault=='authority':item['row']['image_source']['path']='other.elf'
    elif fault=='missing':del sd['providers/scene.elf']
    elif fault in ('fallback','internal-only'):
     boot=json.loads(b['boot.json'])
     if fault=='fallback':boot['drivers'][0]['image_source']['internal_fallback']=False
     else:del boot['drivers'][0]['image_source']
     b['boot.json']=encode(boot);item['row']=copy.deepcopy(boot['drivers'][0])
    p['inventory_sha256']=fingerprint(inventory(b,sd))
    with self.assertRaises(ValueError):overlay(b,sd,p,b'\x7fELFhome',m,root)
 def test_explicit_grant_substitution_preserves_unrelated_authority(self):
  old={'capability':'old.service','api':2,'instance_id':0}
  new={'capability':'service.facade','api':2,'instance_id':0}
  keep={'capability':'storage.key-value','api':1,'instance_id':5}
  manifest={'requires':[],'optional':[{'capability':'service.facade','api':2}]}
  providers={('old.service',2,0):'old',('service.facade',2,0):'new'}
  change={'from':old,'to':new}
  self.assertEqual(validate_grant_substitutions([old,keep],[new,keep],[change],manifest,providers),[old])
  for before,after,changes,m,available in (
    ([old,keep],[new],[change],manifest,providers),
    ([old,keep],[new,keep],[],manifest,providers),
    ([old,keep],[new,keep],[change],manifest,{}),
    ([old,keep],[new,keep],[change,change],manifest,providers),
    ([old,keep],[new,keep],[change],{'requires':[],'optional':[]},providers),
    ([old,keep],[new,keep],[{'from':keep,'to':new}],manifest,providers)):
   with self.assertRaises(ValueError):validate_grant_substitutions(before,after,changes,m,available)
 def test_payload_inventory_exact_delta(self):
  report=payload_inventory({'keep':b'ab','new':b'1234'},{'keep':b'a'},64)
  self.assertEqual(report['store_bytes'],64)
  self.assertEqual(report['payload_bytes'],6)
  self.assertEqual(report['before_payload_bytes'],1)
  self.assertEqual([(x['path'],x['delta_bytes']) for x in report['files']],[('new',4),('keep',1)])
 def test_existing_bootfs_name_boundary(self):
  validate_bootfs_names(('a'*30,'home-policy/manifest.json','home-disp/manifest.json'))
  for name in ('a'*31,'home-presentation/manifest.json','home-display-power/manifest.json','non-ascii-é'):
   with self.subTest(name=name),self.assertRaises(ValueError):validate_bootfs_names((name,))
 def test_pack_refuses_invalid_name_before_builder(self):
  with tempfile.TemporaryDirectory() as tmp:
   def forbidden(*args):raise AssertionError('Builder must not receive oversized path')
   with self.assertRaises(ValueError):pack_candidate(Path(tmp),{'a'*31:b'bytes'},{},32,forbidden,forbidden)
 def test_pack_exact_size_and_sd_roundtrip(self):
  with tempfile.TemporaryDirectory() as tmp:
   entries={'default.elf':b'complete'};sd={'book.epub':b'unchanged'}
   calls=[]
   def build(values,size):
    calls.append((values,size));return b'x'*size,{'fixture':True}
   def read_image(image,size):return entries
   result=pack_candidate(Path(tmp),entries,sd,32,build,read_image)
   self.assertEqual(calls,[(entries,32)]);self.assertTrue(result['store_roundtrip'])
   self.assertEqual((Path(tmp)/'store.bin').stat().st_size,32)
 def test_pack_no_resize_or_tampered_decode(self):
  with tempfile.TemporaryDirectory() as tmp:
   for build,reader in ((lambda e,n:(b'x'*(n+1),{}),lambda b,n:{}),
                        (lambda e,n:(b'x'*n,{}),lambda b,n:{'wrong':b'bytes'})):
    with self.assertRaises(ValueError):pack_candidate(Path(tmp),{'keep':b'data'},{},32,build,reader)
 def test_sd_route_preserved(self):
  b,sd,p,m=self.fixture();row=json.loads(b['boot.json']);row['app_capabilities'][0]['image_source']={'path':'apps/home.elf'}
  b['boot.json']=encode(row);del b['default.elf'];sd['apps/home.elf']=b'old'
  p['inventory_sha256']=fingerprint(inventory(b,sd));a=p['aliases'][0]
  a['route']={'storage':'sd','path':'apps/home.elf'}
  a['original_policy_rows']=[row['app_capabilities'][0]];a['policy_rows']=copy.deepcopy(a['original_policy_rows'])
  out,newsd,r=self.apply(b,sd,p,m)
  self.assertEqual(newsd['apps/home.elf'],b'\x7fELFcomplete-dual-view');self.assertEqual(newsd['books/read.epub'],sd['books/read.epub'])
if __name__=='__main__':unittest.main()

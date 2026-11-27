#!/usr/bin/env python3
"""Content-addressed route regressions. Synthetic inputs are not device proof."""
import copy,json,tempfile,unittest
from pathlib import Path
from assemble_shared_home_cohort import external_target,fingerprint,inventory,overlay
from compose_shared_app_cohort import digest,encode
from prepare_home_cohort_plan import replacement
import test_shared_home_cohort as baseline

class ExternalImages(unittest.TestCase):
 fixture=baseline.Overlay.fixture
 external_provider_fixture=baseline.Overlay.external_provider_fixture

 def provider(self,root,mismatched=False):
  b,sd,p,m,elf,row=self.external_provider_fixture(root)
  old='providers/scene.elf'
  path='providers/image-'+('1'*64 if mismatched else digest(sd[old]))+'.elf'
  sd[path]=sd.pop(old)
  boot=json.loads(b['boot.json']);boot['drivers'][0]['image_source']['path']=path
  b['boot.json']=encode(boot);row=copy.deepcopy(boot['drivers'][0])
  item=p['providers'][0];item['row']=row
  item['replace']['external_route']['path']=path
  target=external_target(path,digest(elf))
  item['replace']['external_route']['target_path']=target
  p['inventory_sha256']=fingerprint(inventory(b,sd))
  return b,sd,p,m,elf,path,target

 def apply(self,b,sd,p,m,root):return overlay(b,sd,p,b'\x7fELFhome',m,root)

 def test_content_addressed_provider_and_inherited_mismatch(self):
  for mismatch in (False,True):
   with self.subTest(mismatch=mismatch),tempfile.TemporaryDirectory() as tmp:
    root=Path(tmp);b,sd,p,m,elf,path,target=self.provider(root,mismatch)
    originals=copy.deepcopy((b,sd,p,m))
    out,new,receipt=self.apply(b,sd,p,m,root)
    oldrow=json.loads(b['boot.json'])['drivers'][0]
    newrow=json.loads(out['boot.json'])['drivers'][0]
    expected=copy.deepcopy(oldrow);expected['image_source']['path']=target
    self.assertEqual(newrow,expected)
    self.assertEqual(out['scene/driver.elf'],elf);self.assertEqual(new[target],elf)
    self.assertEqual(new[path],sd[path]);self.assertEqual(new['books/read.epub'],sd['books/read.epub'])
    self.assertEqual(receipt['changed_sd'],[target]);self.assertEqual(receipt['external_routes'][0]['target_path'],target)
    self.assertEqual((b,sd,p,m),originals)

 def test_reject_unreviewed_or_inconsistent_destination(self):
  for fault in ('omitted','old','other-hash','directory','unsafe','collision','baseline','authority'):
   with self.subTest(fault=fault),tempfile.TemporaryDirectory() as tmp:
    root=Path(tmp);b,sd,p,m,elf,path,target=self.provider(root)
    item=p['providers'][0];route=item['replace']['external_route']
    if fault=='omitted':del route['target_path']
    elif fault=='old':route['target_path']=path
    elif fault=='other-hash':route['target_path']='providers/image-'+'f'*64+'.elf'
    elif fault=='directory':route['target_path']='unreviewed/'+Path(target).name
    elif fault=='unsafe':route['target_path']='../'+target
    elif fault=='collision':sd[target]=b'private unrelated data'
    elif fault=='baseline':route['elf_sha256']='0'*64
    elif fault=='authority':item['row']['image_source']['volume_instance']=10
    p['inventory_sha256']=fingerprint(inventory(b,sd))
    with self.assertRaises(ValueError):self.apply(b,sd,p,m,root)

 def test_identical_existing_destination_is_safe(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);b,sd,p,m,elf,path,target=self.provider(root)
   sd[target]=elf;p['inventory_sha256']=fingerprint(inventory(b,sd))
   out,new,receipt=self.apply(b,sd,p,m,root)
   self.assertEqual(new,sd);self.assertEqual(receipt['changed_sd'],[])
   self.assertEqual(json.loads(out['boot.json'])['drivers'][0]['image_source']['path'],target)

 def test_retains_shared_old_object_and_unselected_row(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);b,sd,p,m,elf,path,target=self.provider(root)
   boot=json.loads(b['boot.json']);other={'manifest':'clock.json','grants':[],
     'image_source':{'path':path,'volume_instance':9}}
   boot['app_capabilities'][1]=other;b['boot.json']=encode(boot)
   p['inventory_sha256']=fingerprint(inventory(b,sd))
   out,new,_=self.apply(b,sd,p,m,root)
   self.assertEqual(json.loads(out['boot.json'])['app_capabilities'][1],other)
   self.assertEqual(new[path],sd[path]);self.assertEqual(new[target],elf)

 def test_stable_provider_must_not_overwrite_unselected_reference(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);b,sd,p,m,elf,row=self.external_provider_fixture(root)
   boot=json.loads(b['boot.json']);boot['app_capabilities'][1]['image_source']={'path':'providers/scene.elf'}
   b['boot.json']=encode(boot);p['inventory_sha256']=fingerprint(inventory(b,sd))
   with self.assertRaisesRegex(ValueError,'unselected route'):self.apply(b,sd,p,m,root)

 def app(self):
  b,sd,p,m=self.fixture();elf=b'\x7fELFhome'
  old='apps/image-'+digest(b'old')+'.elf';target=external_target(old,digest(elf))
  sd[old]=b'old'
  boot=json.loads(b['boot.json']);row=boot['app_capabilities'][0]
  row['image_source']={'path':old,'volume_instance':9,'internal_fallback':True}
  b['boot.json']=encode(boot);alias=p['aliases'][0]
  alias['route']={'storage':'sd','path':old,'target_path':target}
  alias['original_policy_rows']=[copy.deepcopy(row)];alias['policy_rows']=[copy.deepcopy(row)]
  alias['policy_rows'][0]['image_source']['path']=target
  p['inventory_sha256']=fingerprint(inventory(b,sd))
  return b,sd,p,m,old,target

 def test_application_route_preserves_authority_and_old_bytes(self):
  b,sd,p,m,old,target=self.app();out,new,r=self.apply(b,sd,p,m,Path('.'))
  row=json.loads(out['boot.json'])['app_capabilities'][0]
  self.assertEqual(row,p['aliases'][0]['policy_rows'][0]);self.assertEqual(new[old],sd[old])
  self.assertEqual(new[target],b'\x7fELFhome');self.assertEqual(r['routes'],[['internal','default.elf'],['sd',target]])
  self.assertEqual(json.loads(out['default.json'])['label'],'My Home')
  self.assertEqual(out['saved/points'],b['saved/points'])

 def test_app_requires_explicit_target_in_both_route_and_policy(self):
  for fault in ('route','policy','volume','grants','internal'):
   with self.subTest(fault=fault):
    b,sd,p,m,old,target=self.app();a=p['aliases'][0]
    if fault=='route':del a['route']['target_path']
    elif fault=='policy':a['policy_rows'][0]['image_source']['path']=old
    elif fault=='volume':a['policy_rows'][0]['image_source']['volume_instance']=99
    elif fault=='grants':a['policy_rows'][0]['grants']=[]
    elif fault=='internal':a['route']['storage']='internal'
    with self.assertRaises((ValueError,KeyError)):self.apply(b,sd,p,m,Path('.'))

 def test_application_fallback_tracks_same_canonical_elf(self):
  b,sd,p,m,old,target=self.app();b['default.elf']=b'old-fallback'
  boot=json.loads(b['boot.json']);boot['app_capabilities'][0]['image_source']['internal_fallback']=True
  b['boot.json']=encode(boot)
  for key in ('original_policy_rows','policy_rows'):
   p['aliases'][0][key][0]['image_source']['internal_fallback']=True
  p['inventory_sha256']=fingerprint(inventory(b,sd))
  out,new,r=self.apply(b,sd,p,m,Path('.'))
  self.assertEqual(out['default.elf'],new[target]);self.assertEqual(out['default.elf'],b'\x7fELFhome')
  self.assertIn(['internal','default.elf'],r['routes'])
  del b['default.elf'];p['inventory_sha256']=fingerprint(inventory(b,sd))
  with self.assertRaisesRegex(ValueError,'fallback missing'):self.apply(b,sd,p,m,Path('.'))

 def test_internal_alias_must_not_modify_unselected_application(self):
  for external in (False,True):
   with self.subTest(external=external):
    if external:
     b,sd,p,m,old,target=self.app();b['default.elf']=b'old-fallback'
     boot=json.loads(b['boot.json']);boot['app_capabilities'][0]['image_source']['internal_fallback']=True
     for key in ('original_policy_rows','policy_rows'):p['aliases'][0][key][0]['image_source']['internal_fallback']=True
    else:b,sd,p,m=self.fixture();boot=json.loads(b['boot.json'])
    b['other.json']=encode({'type':'application','id':'other','file_name':'default.elf'})
    boot['app_capabilities'].append({'manifest':'other.json','grants':[]});b['boot.json']=encode(boot)
    p['inventory_sha256']=fingerprint(inventory(b,sd))
    with self.assertRaisesRegex(ValueError,'Internal overwrite.*unselected'):self.apply(b,sd,p,m,Path('.'))

 def test_existing_corrupt_content_address_is_not_overwritten(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);b,sd,p,m,elf,path,target=self.provider(root)
   sd[target]=sd.pop(path);boot=json.loads(b['boot.json']);boot['drivers'][0]['image_source']['path']=target
   b['boot.json']=encode(boot);p['providers'][0]['row']=copy.deepcopy(boot['drivers'][0])
   p['providers'][0]['replace']['external_route']['path']=target
   p['inventory_sha256']=fingerprint(inventory(b,sd))
   with self.assertRaisesRegex(ValueError,'destination collision'):self.apply(b,sd,p,m,root)

 def test_external_authority_comparison_preserves_json_types(self):
  for value in (9.0,True):
   with self.subTest(value=value):
    b,sd,p,m,old,target=self.app()
    p['aliases'][0]['policy_rows'][0]['image_source']['volume_instance']=value
    with self.assertRaisesRegex(ValueError,'Only explicit'):self.apply(b,sd,p,m,Path('.'))
   with tempfile.TemporaryDirectory() as tmp:
    root=Path(tmp);b,sd,p,m,elf,path,target=self.provider(root)
    p['providers'][0]['row']['image_source']['volume_instance']=value
    with self.assertRaisesRegex(ValueError,'authority differs'):self.apply(b,sd,p,m,root)

 def test_unmirrored_content_addressed_app_retarget_is_refused(self):
  for present in (False,None):
   with self.subTest(fallback=present):
    b,sd,p,m,old,target=self.app();boot=json.loads(b['boot.json'])
    sources=[boot['app_capabilities'][0]['image_source']]+[p['aliases'][0][key][0]['image_source'] for key in ('original_policy_rows','policy_rows')]
    for source in sources:
     if present is None:source.pop('internal_fallback')
     else:source['internal_fallback']=present
    b['boot.json']=encode(boot);p['inventory_sha256']=fingerprint(inventory(b,sd))
    with self.assertRaisesRegex(ValueError,'retargeting requires'):self.apply(b,sd,p,m,Path('.'))

 def test_independent_app_update_uses_same_route_rules(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);b,sd,p,m=self.fixture();pkg=root/'app';pkg.mkdir()
   old='apps/image-'+digest(b'old-app')+'.elf';elf=b'\x7fELFindependent'
   target=external_target(old,digest(elf));sd[old]=b'old-app'
   manifest={**m,'id':'separate','label':'Preserved','file_name':'separate.elf'}
   b['separate.json']=encode(manifest);b['separate.elf']=b'old-app'
   row={'manifest':'separate.json','grants':[],'image_source':{'path':old,'volume_instance':9,'internal_fallback':True}}
   newrow=copy.deepcopy(row);newrow['image_source']['path']=target
   boot=json.loads(b['boot.json']);boot['app_capabilities'].append(row);b['boot.json']=encode(boot)
   (pkg/'manifest.json').write_bytes(encode(manifest));(pkg/'app.elf').write_bytes(elf)
   p['app_updates']=[{'manifest':'separate.json','package':'app','manifest_file':'manifest.json','elf_file':'app.elf',
     'manifest_sha256':digest(encode(manifest)),'elf_sha256':digest(elf),
     'route':{'storage':'sd','path':old,'target_path':target},'original_policy_rows':[row],'policy_rows':[newrow],
     'replace':{'manifest_sha256':digest(b['separate.json']),'elf_sha256':digest(sd[old])}}]
   p['inventory_sha256']=fingerprint(inventory(b,sd))
   out,new,receipt=self.apply(b,sd,p,m,root)
   self.assertEqual(new[target],elf);self.assertEqual(new[old],sd[old])
   self.assertEqual(json.loads(out['boot.json'])['app_capabilities'][-1],newrow)
   self.assertEqual(receipt['external_routes'][0]['manifest'],'separate.json')

 def test_plan_binds_actual_target_and_external_baseline(self):
  for mismatch in (False,True):
   with self.subTest(mismatch=mismatch),tempfile.TemporaryDirectory() as tmp:
    root=Path(tmp);b,sd,p,m,elf,path,target=self.provider(root,mismatch)
    item=replacement(root,'selected','scene/manifest.json',b,sd,json.loads(b['boot.json']))
    self.assertEqual(item['replace']['external_route'],{'path':path,'target_path':target,'elf_sha256':digest(sd[path])})
    self.assertEqual(item['row'],json.loads(b['boot.json'])['drivers'][0])
    p['providers']=[item];self.apply(b,sd,p,m,root)

 def test_route_classification_is_general_not_product_specific(self):
  sha=digest(b'any ELF')
  for parent in ('','a/','a/deep/path/'):
   old=parent+'image-'+'0'*64+'.elf'
   self.assertEqual(external_target(old,sha),parent+'image-'+sha+'.elf')
  for stable in ('scene.elf','providers/other.elf','image-loader.elf'):
   self.assertEqual(external_target(stable,sha),stable)
  for bad in ('', 'short', 'A'*64):
   with self.assertRaises(ValueError):external_target('scene.elf',bad)

if __name__=='__main__':unittest.main()

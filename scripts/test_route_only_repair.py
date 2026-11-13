#!/usr/bin/env python3
import copy,json,tempfile,unittest,subprocess,sys
from pathlib import Path
from compose_shared_app_cohort import digest,encode,write_tree
from repair_shared_external_routes import prepare,apply

class RouteRepair(unittest.TestCase):
 def fixture(self):
  entries={'private/saved':b'unchanged private bytes','cohort.json':encode({'version':'same','firmware_sha256':'same'})}
  sd={'books/read.epub':b'book'};boot={'board':'board.json','drivers':[],'app_capabilities':[],'namespace_map':{'saved':5}}
  entries['board.json']=encode({'id':'board'})
  for section,kind,folder,ident in [('drivers','driver','Drivers','scene'),('app_capabilities','application','Apps','home')]:
   manifest=ident+'.json';binary=ident+'.elf';data=b'\x7fELF'+ident.encode()
   entries[manifest]=encode({'id':ident,'type':kind,'file_name':binary,'version':'1.0.0'})
   entries[binary]=data;old=folder+'/'+ident+'/image-'+'0'*64+'.elf';sd[old]=data
   boot[section].append({'manifest':manifest,'instance_id':0,'bindings':[],
     'image_source':{'path':old,'volume_instance':9,'internal_fallback':True}})
  entries['boot.json']=encode(boot);return entries,sd

 def test_driver_and_app_repair_preserves_all_other_bytes_and_authority(self):
  entries,sd=self.fixture();original=copy.deepcopy((entries,sd));plan=prepare(entries,sd)
  self.assertEqual(len(plan['repairs']),2)
  out,new,r=apply(entries,sd,plan);oldboot=json.loads(entries['boot.json']);newboot=json.loads(out['boot.json'])
  for section in ('drivers','app_capabilities'):
   a=oldboot[section][0];b=newboot[section][0];target=b['image_source']['path']
   a['image_source']['path']=target;self.assertEqual(a,b)
   manifest=json.loads(entries[a['manifest']]);self.assertEqual(new[target],entries[manifest['file_name']])
  self.assertEqual(oldboot,newboot)
  self.assertEqual({k:v for k,v in out.items() if k!='boot.json'},{k:v for k,v in entries.items() if k!='boot.json'})
  self.assertEqual({k:new[k] for k in sd},sd);self.assertEqual((entries,sd),original)
  self.assertEqual(r['changed_internal'],['boot.json']);self.assertEqual(r['removed_sd'],[])
  self.assertFalse(r['runtime_policy_tested']);self.assertFalse(r['hardware_tested'])

 def test_already_consistent_is_byte_exact_noop(self):
  entries,sd=self.fixture();out,new,_=apply(entries,sd,prepare(entries,sd))
  plan=prepare(out,new);self.assertEqual(plan['repairs'],[])
  after,s,r=apply(out,new,plan);self.assertEqual((after,s),(out,new));self.assertEqual(r['changed_internal'],[])

 def test_shared_old_object_multiple_instances(self):
  entries,sd=self.fixture();boot=json.loads(entries['boot.json'])
  row=copy.deepcopy(boot['drivers'][0]);row['instance_id']=1;boot['drivers'].append(row);entries['boot.json']=encode(boot)
  plan=prepare(entries,sd);self.assertEqual(len(plan['repairs']),3)
  out,new,_=apply(entries,sd,plan);boot=json.loads(out['boot.json'])
  self.assertEqual(boot['drivers'][0]['image_source']['path'],boot['drivers'][1]['image_source']['path'])
  self.assertEqual({k:new[k] for k in sd},sd)

 def test_reject_stale_duplicate_incomplete_or_changed_authority(self):
  for fault in ('inventory','omit','duplicate','row','row-type','hash','internal','manifest','target','index','unsafe'):
   with self.subTest(fault=fault):
    entries,sd=self.fixture();plan=prepare(entries,sd);item=plan['repairs'][0]
    if fault=='inventory':entries['private/saved']=b'new state'
    elif fault=='omit':plan['repairs'].pop()
    elif fault=='duplicate':plan['repairs'].append(copy.deepcopy(item))
    elif fault=='row':item['row']['image_source']['volume_instance']=10
    elif fault=='row-type':item['row']['image_source']['volume_instance']=9.0
    elif fault=='hash':item['external_route']['elf_sha256']='f'*64
    elif fault=='internal':item['internal_sha256']='f'*64
    elif fault=='manifest':item['manifest_sha256']='f'*64
    elif fault=='target':item['external_route']['target_path']='Drivers/other/image-'+'f'*64+'.elf'
    elif fault=='index':item['index']=True
    elif fault=='unsafe':item['external_route']['target_path']='../outside'
    original=copy.deepcopy((entries,sd))
    with self.assertRaises(ValueError):apply(entries,sd,plan)
    self.assertEqual((entries,sd),original)

 def test_collision_rejected_without_changing_inputs(self):
  entries,sd=self.fixture();plan=prepare(entries,sd)
  sd[plan['repairs'][0]['external_route']['target_path']]=b'unrelated'
  plan=prepare(entries,sd);original=copy.deepcopy((entries,sd))
  with self.assertRaisesRegex(ValueError,'collision'):apply(entries,sd,plan)
  self.assertEqual((entries,sd),original)

 def test_canonical_corrupt_object_rejected(self):
  entries,sd=self.fixture();out,new,_=apply(entries,sd,prepare(entries,sd))
  path=json.loads(out['boot.json'])['drivers'][0]['image_source']['path'];new[path]=b'corrupt'
  with self.assertRaisesRegex(ValueError,'collision'):apply(out,new,prepare(out,new))

 def test_absent_but_correctly_addressed_optional_image_remains_absent(self):
  entries,sd=self.fixture();out,new,_=apply(entries,sd,prepare(entries,sd))
  path=json.loads(out['boot.json'])['drivers'][0]['image_source']['path'];del new[path]
  plan=prepare(out,new);self.assertEqual(plan['repairs'],[])
  result,external,_=apply(out,new,plan);self.assertEqual(result,out);self.assertEqual(external,new)
  entries,sd=self.fixture();path=json.loads(entries['boot.json'])['drivers'][0]['image_source']['path'];del sd[path]
  with self.assertRaisesRegex(ValueError,'Mismatched preferred'):prepare(entries,sd)

 def test_reject_non_runtime_path_shape(self):
  for bad in ('providers/image-'+'0'*64+'.elf','Drivers/wrong/image-'+'0'*64+'.elf','Drivers/scene/image-'+'A'*64+'.elf'):
   with self.subTest(path=bad):
    entries,sd=self.fixture();boot=json.loads(entries['boot.json']);row=boot['drivers'][0]
    sd[bad]=sd.pop(row['image_source']['path']);row['image_source']['path']=bad;entries['boot.json']=encode(boot)
    with self.assertRaisesRegex(ValueError,'content path'):prepare(entries,sd)

 def test_unmirrored_legacy_routes_remain_unchanged(self):
  entries,sd=self.fixture();boot=json.loads(entries['boot.json']);row=boot['app_capabilities'][0]
  row['image_source']['internal_fallback']=False;entries['boot.json']=encode(boot)
  plan=prepare(entries,sd);self.assertEqual(len(plan['repairs']),1)
  out,_,_=apply(entries,sd,plan);self.assertEqual(json.loads(out['boot.json'])['app_capabilities'][0],row)

 def test_runtime_identity_and_filename_bounds(self):
  for field,value in [('id','x'*64),('id','Upper'),('id','trailing-'),('id','has.dot'),
                      ('file_name','a'*124+'.elf'),('file_name','bad..elf'),('file_name','.elf')]:
   with self.subTest(field=field,value=value):
    entries,sd=self.fixture();m=json.loads(entries['scene.json']);m[field]=value;entries['scene.json']=encode(m)
    with self.assertRaises(ValueError):prepare(entries,sd)
  entries,sd=self.fixture();boot=json.loads(entries['boot.json']);long='a'*183+'/scene.json'
  entries[long]=entries.pop('scene.json');boot['drivers'][0]['manifest']=long;entries['boot.json']=encode(boot)
  with self.assertRaisesRegex(ValueError,'buffer bound'):prepare(entries,sd)

 def test_cli_rejects_before_output_exists(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);entries,sd=self.fixture();plan=prepare(entries,sd);plan['repairs'].pop()
   write_tree(root/'internal',entries);write_tree(root/'sd',sd);(root/'plan.json').write_bytes(encode(plan))
   result=subprocess.run([sys.executable,str(Path(__file__).with_name('repair_shared_external_routes.py')),'apply',
     '--internal',str(root/'internal'),'--sd',str(root/'sd'),'--plan',str(root/'plan.json'),'--output',str(root/'output')],capture_output=True,timeout=10)
   self.assertNotEqual(result.returncode,0);self.assertFalse((root/'output').exists())

if __name__=='__main__':unittest.main()

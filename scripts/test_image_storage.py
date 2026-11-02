#!/usr/bin/env python3
import json,unittest
from compose_shared_app_cohort import digest
from image_storage import ordinary_images,apply
class Storage(unittest.TestCase):
 def fixture(self):
  data={'default.elf':b'\x7fELF'+b'a'*4092,'driver.elf':b'\x7fELF'+b'b'*4092,'mirror.elf':b'\x7fELF'+b'c'*4092,'data.bin':b'preserved'}
  for path,typ,filename in [('default.json','application','default.elf'),('driver.json','driver','driver.elf'),('mirror.json','application','mirror.elf')]:
   data[path]=json.dumps({'type':typ,'file_name':filename}).encode()
  data['boot.json']=json.dumps({'drivers':[{'manifest':'driver.json'}],'app_capabilities':[{'manifest':'default.json','grants':[]},{'manifest':'mirror.json','image_source':{'path':'Apps/mirror.elf','internal_fallback':True}}]}).encode()
  return data
 def test_exact_recovery_and_unrelated_bytes(self):
  data=self.fixture();self.assertEqual(ordinary_images(data),{'default.elf'})
  out,r=apply(data,['default.elf'],{'default.elf':digest(data['default.elf'])})
  self.assertTrue(r['canonical_recovery_verified']);self.assertLess(len(out['default.elf']),len(data['default.elf']))
  for name in data:
   if name!='default.elf':self.assertEqual(out[name],data[name])
 def test_forbidden_roles_and_stale_baselines(self):
  data=self.fixture()
  for names,pins in [(['driver.elf'],{'driver.elf':digest(data['driver.elf'])}),(['mirror.elf'],{'mirror.elf':digest(data['mirror.elf'])}),(['data.bin'],{'data.bin':digest(data['data.bin'])}),(['default.elf'],{'default.elf':'stale'}),(['default.elf','default.elf'],{'default.elf':digest(data['default.elf'])}),([] ,{'default.elf':'unused'})]:
   with self.assertRaises(ValueError):apply(data,names,pins)
 def test_driver_app_alias_is_not_eligible(self):
  data=self.fixture();boot=json.loads(data['boot.json']);boot['drivers'].append({'manifest':'default.json'});data['boot.json']=json.dumps(boot).encode()
  self.assertEqual(ordinary_images(data),set())
if __name__=='__main__':unittest.main()

#!/usr/bin/env python3
"""Archive contract tests only; no product runtime claim."""
import hashlib,json,tempfile,unittest,zipfile
from pathlib import Path
import stage_home_x4_bridge as stage
class Staging(unittest.TestCase):
 def make(self,root,change=None):
  cases='enter-owner-deep enter-owner-light enter-owner-display enter-owner-display-resume enter-take-deep enter-take-light enter-take-display enter-take-display-resume enter-take-nested enter-take-nested-unknown enter-owner-display-unknown'.split()
  receipt={'source':stage.LOCK['source'],'physical_hardware_tested':False,'runs':[{'mode':m,'case':c} for m in ('normal','sanitized') for c in cases],'products':[],'sha256':{}}
  data={}
  for name in stage.PACKAGES:
   raw=b'\x7fELFfixture'+name.encode();manifest=json.dumps({'id':name}).encode()
   data['singleton/target/'+name+'/driver.elf']=raw
   data['singleton/target/'+name+'/manifest.json']=manifest
   receipt['products'].append({'id':name,'bytes':len(raw),'sha256':stage.digest(raw)})
   receipt['sha256']['minimal/drivers/'+name+'/manifest.json']=stage.digest(manifest)
  if change:change(receipt,data)
  data['singleton/receipt.json']=json.dumps(receipt).encode()
  for name in ('x4','runtime','reader','watch','system','utilities'):
   data['source/'+name+'.head']=(stage.LOCK['source']+'\n').encode()
   data['source/'+name+'.tar.gz']=b'fixture source archive'
  path=root/'fixture.zip'
  with zipfile.ZipFile(path,'w') as z:
   for name,raw in data.items():z.writestr(name,raw)
  stage.LOCK['sha256']=stage.digest(path.read_bytes())
  return path
 def test_exact_closure(self):
  with tempfile.TemporaryDirectory() as t:
   root=Path(t);path=self.make(root);stage.stage(path,root/'out')
   self.assertEqual(set(p.name for p in (root/'out/packages').iterdir()),set(stage.PACKAGES.values()))
 def test_mismatches(self):
  changes=[lambda r,d:r.update(source='0'*40),
    lambda r,d:r['runs'].pop(),
    lambda r,d:d.update({'singleton/target/x4pro_power/driver.elf':b'changed'}),
    lambda r,d:d.update({'../escape':b'no'}),
    lambda r,d:r['products'].pop()]
  for change in changes:
   with self.subTest(change=change),tempfile.TemporaryDirectory() as t:
    root=Path(t);path=self.make(root,change)
    with self.assertRaises(ValueError):stage.stage(path,root/'out')
    self.assertFalse((root/'out').exists())
 def test_archive_hash(self):
  with tempfile.TemporaryDirectory() as t:
   root=Path(t);path=self.make(root);stage.LOCK['sha256']='0'*64
   with self.assertRaises(ValueError):stage.stage(path,root/'out')
if __name__=='__main__':unittest.main()

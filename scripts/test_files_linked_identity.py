#!/usr/bin/env python3
import hashlib,json,unittest
from link_files_scene import make_manifest,verified_linked_manifest
class Identity(unittest.TestCase):
 def pair(self,version):
  m=make_manifest(version,1);b=(json.dumps(m,indent=2)+'\n').encode()
  return b,{'version':version,'manifest_sha256':hashlib.sha256(b).hexdigest()}
 def test_successor_version(self):
  for v in ('1.6.1','2.0.0','10.4.7'):
   b,r=self.pair(v);m=verified_linked_manifest(b,r)
   for alarm in (1,2):self.assertEqual(make_manifest(m['version'],alarm)['version'],v)
 def test_manifest_hash_mismatch(self):
  b,r=self.pair('1.6.1')
  with self.assertRaises(ValueError):verified_linked_manifest(b+b' ',r)
 def test_receipt_version_mismatch(self):
  b,r=self.pair('1.6.1');r['version']='1.6.0'
  with self.assertRaises(ValueError):verified_linked_manifest(b,r)
 def test_declaration_drift(self):
  b,r=self.pair('1.6.1');m=json.loads(b);m['requires']=[{'capability':'invented','api':1}];b=json.dumps(m).encode();r['manifest_sha256']=hashlib.sha256(b).hexdigest()
  with self.assertRaises(ValueError):verified_linked_manifest(b,r)
 def test_missing_receipt_binding(self):
  b,r=self.pair('1.6.1');del r['manifest_sha256']
  with self.assertRaises(KeyError):verified_linked_manifest(b,r)
if __name__=='__main__':unittest.main()

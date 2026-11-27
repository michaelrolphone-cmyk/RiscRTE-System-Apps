#!/usr/bin/env python3
import copy, importlib.util, json, tempfile, unittest, zipfile
from pathlib import Path
spec=importlib.util.spec_from_file_location('derive',Path(__file__).with_name('derive_files_product_authority.py'))
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class DerivationTests(unittest.TestCase):
 def test_verbatim_authority(self):
  rows=[{'manifest':'file_browser.json','grants':[{'capability':'storage.volume','api':1,'instance_id':17}],'namespaces':{'a':'b'}},{'manifest':'other.json','grants':[]}]
  boot=(json.dumps({'app_capabilities':rows})+'\n').encode()
  files={'boot.json':boot,'file_browser.json':b'{"id":"file_browser","version":"1.0.0"}'}
  old=copy.deepcopy(files);r=m.product_record(files)
  self.assertEqual(r['rows'],rows[:1]);self.assertEqual(files,old)
  self.assertEqual(r['boot_sha256'],m.hashlib.sha256(boot).hexdigest())
 def test_absence_rejected(self):
  with self.assertRaises(ValueError):m.product_record({'boot.json':b'{"app_capabilities":[]}','file_browser.json':b'{}'})
 def test_archive_names(self):
  for name in ('../bad','/bad','a\\b'):
   with self.subTest(name=name),tempfile.TemporaryDirectory() as d:
    p=Path(d)/'a.zip'
    with zipfile.ZipFile(p,'w') as z:z.writestr('candidate/store.bin',b'original');z.writestr(name,b'x')
    with self.assertRaises(ValueError):m.read_member(p,'candidate/store.bin')
 def test_exact_member(self):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/'a.zip'
   with zipfile.ZipFile(p,'w') as z:z.writestr('candidate/store.bin',b'original')
   self.assertEqual(m.read_member(p,'candidate/store.bin'),b'original')
if __name__=='__main__': unittest.main()

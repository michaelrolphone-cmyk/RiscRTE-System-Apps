import importlib.util,json,shutil,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
spec=importlib.util.spec_from_file_location('watch_alarm_source',ROOT/'scripts/verify_watch_alarm_source.py')
source=importlib.util.module_from_spec(spec);spec.loader.exec_module(source)
class WatchAlarmSourceTests(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);self.path=Path(self.tmp.name)
  for name in ('manifest.json','System.bundle'):shutil.copyfile(source.CUSTODY/name,self.path/name)
 def test_fixed_bundle(self):self.assertEqual(source.read_custody(self.path)[0]['source_commit'],source.SOURCE)
 def test_corruption(self):
  path=self.path/'System.bundle';data=bytearray(path.read_bytes());data[-1]^=1;path.write_bytes(data)
  with self.assertRaisesRegex(ValueError,'SHA-256'):source.read_custody(self.path)
 def test_truncation(self):
  path=self.path/'System.bundle';path.write_bytes(path.read_bytes()[:-1])
  with self.assertRaisesRegex(ValueError,'byte count'):source.read_custody(self.path)
 def test_mapping_change(self):
  path=self.path/'manifest.json';data=json.loads(path.read_text());data['prerequisite_commit']=source.PUBLIC;path.write_text(json.dumps(data))
  with self.assertRaisesRegex(ValueError,'fixed source mapping'):source.read_custody(self.path)

import importlib.util,pathlib,tempfile,argparse,unittest,subprocess
ROOT=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('editor_sdk',ROOT/'scripts/portable_alarm_build.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class EditorBuild(unittest.TestCase):
 def test_old_anchor_stable(self):
  self.assertEqual(m.UTILITIES_COMMIT,'e80172353fcb9e6519ca9d9b2a4d43c69c1bc91d');self.assertEqual(m.HEADERS,('AlarmServiceV1.h','AlarmServiceV2.h'))
 def test_editor_anchor_explicit(self):
  self.assertEqual(m.CLOCK_EDIT_COMMIT,'bbafc2c3aebe8c3b9805639f873341e31296c8fb');self.assertIn('AlarmClockEditV2.h',m.CLOCK_EDIT_HEADERS)
 def test_orphan_editor_rejected(self):
  with tempfile.TemporaryDirectory() as tmp:
   with self.assertRaises(SystemExit):m.stage(argparse.Namespace(clock_edit_authority=True,tagged_alarm_utilities=None,alarm_client=True),argparse.ArgumentParser(),pathlib.Path(tmp),pathlib.Path(tmp))
 def test_default_editor_absent(self):
  self.assertIsNone(m.stage(argparse.Namespace(tagged_alarm_utilities=None),argparse.ArgumentParser(),pathlib.Path('/unused'),pathlib.Path('/unused')))
if __name__=='__main__':unittest.main()

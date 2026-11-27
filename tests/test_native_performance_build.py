import argparse,contextlib,io,json,tempfile,unittest,sys
from pathlib import Path
from types import SimpleNamespace
from unittest import mock
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
import portable_native_performance_build as p
class NativeTraceBuild(unittest.TestCase):
 def args(self,**kw):return SimpleNamespace(**dict(native_performance_trace=True,native_time_runtime_repo=Path('/runtime'),performance_runtime_repo=None,performance_display_sdk=None,stage_logs=False,**kw))
 def test_default_off_is_noop(self):
  a=SimpleNamespace();parser=argparse.ArgumentParser()
  with mock.patch.object(p.subprocess,'check_output') as read:
   p.validate(a,parser,'default');self.assertEqual(p.configure(a,parser,Path('/absent')),([],None));read.assert_not_called()
 def test_selection_and_incompatible_profiles(self):
  parser=argparse.ArgumentParser()
  for profile in ['watch-native-time','native-time-touch']:p.validate(self.args(),parser,profile)
  for profile in ['default','x4-native-time']:
   with contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):p.validate(self.args(),parser,profile)
  for key,value in [('performance_runtime_repo',Path('/old')),('stage_logs',True),('performance_display_sdk',Path('/metrics')),('native_time_runtime_repo',None)]:
   a=self.args();setattr(a,key,value)
   with contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):p.validate(a,parser,'watch-native-time')
 def test_exact_sdk_and_receipt(self):
  parser=argparse.ArgumentParser()
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);(root/'RiscRuntimeV1.h').write_bytes(b'current');(root/'untouched').write_bytes(b'profile')
   # This fixture isolates header staging. The SDK validator has its own real
   # Git repository tests; keep its receipt visible here instead of consuming
   # this two-header mock as though it were a Git repository.
   identity={'canonical_commit':p.RUNTIME_COMMIT,'checkout_commit':p.RUNTIME_COMMIT,
             'checkout_clean':True,'canonical_ancestor':True}
   with mock.patch.object(p,'verify_runtime_sdk',return_value=identity) as verify, mock.patch.object(p.subprocess,'check_output',side_effect=[b'current',b'trace']) as read:
    flags,receipt=p.configure(self.args(),parser,root)
   verify.assert_called_once_with(Path('/runtime'),p.RUNTIME_COMMIT)
   self.assertEqual(receipt['runtime_sdk_identity'],identity)
   self.assertEqual(flags,[p.DEFINE]);self.assertFalse(receipt['profile_changed']);self.assertFalse(receipt['version_changed']);self.assertEqual((root/'untouched').read_bytes(),b'profile');self.assertEqual((root/'RiscPerformanceV1.h').read_bytes(),b'trace')
   self.assertTrue(all(p.RUNTIME_COMMIT+':sdk/app/' in c.args[0][-1] for c in read.call_args_list))
   (root/'RiscPerformanceV1.h').unlink()
   with mock.patch.object(p,'verify_runtime_sdk',return_value=identity),mock.patch.object(p.subprocess,'check_output',side_effect=[b'other',b'trace']),contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):p.configure(self.args(),parser,root)
   self.assertFalse((root/'RiscPerformanceV1.h').exists())
   with mock.patch.object(p,'verify_runtime_sdk',side_effect=ValueError('changed canonical SDK')),mock.patch.object(p.subprocess,'check_output') as read,contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):p.configure(self.args(),parser,root)
   read.assert_not_called()
   self.assertFalse((root/'RiscPerformanceV1.h').exists())
 def test_builders_wire_optin_after_final_staging(self):
  for app in ['settings','springboard']:
   source=(ROOT/f'scripts/build_portable_{app}.py').read_text();self.assertIn('portable_native_performance_build.options(parser)',source);self.assertIn('portable_native_performance_build.validate(',source)
   self.assertLess(source.index('includes,idle_flags,idle_sources='),source.index('native_perf_flags,native_perf_receipt='));self.assertIn('portable_native_performance_build.record(ROOT,native_perf_receipt,record)',source)
if __name__=='__main__':unittest.main()

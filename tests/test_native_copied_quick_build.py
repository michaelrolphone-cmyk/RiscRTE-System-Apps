import argparse,importlib.util,json,tempfile,unittest,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
import portable_quick_build as q
class NativeCopiedQuickBuild(unittest.TestCase):
 def parser(self):
  p=argparse.ArgumentParser();q.options(p);p.add_argument('--alarm-client',action='store_true');p.add_argument('--watch-native-ui',action='store_true');p.add_argument('--time-profile',default='default');p.add_argument('--native-time-runtime-repo',type=Path);return p
 def test_selection(self):
  with tempfile.TemporaryDirectory() as td:
   o=Path(td);sdk=o/'runtime/sdk/app';sdk.mkdir(parents=True);inc=o/'include';inc.mkdir()
   for n in ['RiscPreferenceStoreV1.h','RiscKeyValueAtomicV1.h']:(sdk/n).write_text('// bounded fixture header '+n)
   p=self.parser();a=p.parse_args(['--quick-actions','--native-copied-quick','--alarm-client','--watch-native-ui','--time-profile','watch-native-time','--native-time-runtime-repo',str(o/'runtime')])
   flags,sources=q.configure(a,p,ROOT,o,inc);self.assertIn('-DPORTABLE_QUICK_SLICED_NATIVE',flags);self.assertIn('-DPORTABLE_PREFERENCE_STORE',flags);self.assertEqual(len(sources),3)
   needs=[{'capability':'rtc.clock','api':2}];q.requirements(a,needs);self.assertNotIn('rtc.clock',[x['capability'] for x in needs]);self.assertIn({'capability':'storage.preferences','api':1},needs)
   rec={'required_grants':[{'capability':'storage.preferences','api':1,'instance_id':0}]};q.record(a,rec);self.assertEqual(rec['grant_bindings']['storage.preferences'],[1]);self.assertEqual(rec['required_grants'],[{'capability':'storage.preferences','api':1,'instance_id':1}]);self.assertFalse(rec['native_copied_quick']['resident_architecture_complete']);self.assertFalse(rec['native_copied_quick']['radio_control_grants'])
 def test_default_unchanged(self):
  p=self.parser();a=p.parse_args([]);flags,sources=q.configure(a,p,ROOT,ROOT,None);self.assertEqual((flags,sources),([],[]));rec={};q.record(a,rec);self.assertNotIn('native_copied_quick',rec)
 def test_reject_unqualified(self):
  for args in [['--native-copied-quick'],['--native-copied-quick','--quick-actions','--alarm-client'],['--native-copied-quick','--quick-actions','--alarm-client','--watch-native-ui','--time-profile','watch-native-time','--quick-radios']]:
   p=self.parser();a=p.parse_args(args)
   with self.assertRaises(SystemExit):q.configure(a,p,ROOT,ROOT,None)
if __name__=='__main__':unittest.main()

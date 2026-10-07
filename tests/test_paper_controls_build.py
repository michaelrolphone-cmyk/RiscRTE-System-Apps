import argparse,json,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
import portable_quick_build as quick
class PaperControlsBuild(unittest.TestCase):
 def args(self,*flags):
  p=argparse.ArgumentParser();p.add_argument('--alarm-client',action='store_true');quick.options(p)
  return p,p.parse_args(flags)
 def test_explicit_home_and_paper_controls_add_no_radio_selection(self):
  p,a=self.args('--alarm-client','--quick-actions','--home-app','default.elf')
  with tempfile.TemporaryDirectory() as d:
   flags,sources=quick.configure(a,p,ROOT,Path(d));needs=[];quick.requirements(a,needs)
   self.assertIn('-DPORTABLE_HOME_APP="default.elf"',flags);self.assertIn('-DPORTABLE_QUICK_ACTIONS',flags)
   self.assertNotIn('-DPORTABLE_QUICK_RADIOS',flags)
   self.assertEqual([s.name for s in sources],['quick_actions.c','quick_render.c','quick_session.c'])
   self.assertEqual({s['capability'] for s in needs},{'storage.key-value','rtc.clock','board.battery'})
   self.assertTrue((Path(d)/'licenses/quick-controls/FAClassic-LICENSE.txt').exists())
 def test_radio_selection_is_explicit(self):
  p,a=self.args('--alarm-client','--quick-actions','--quick-radios')
  with tempfile.TemporaryDirectory() as d:
   flags,sources=quick.configure(a,p,ROOT,Path(d));needs=[];quick.requirements(a,needs)
   self.assertIn('-DPORTABLE_QUICK_RADIOS',flags);self.assertEqual(sources[-1].name,'quick_radios.c')
   self.assertIn({'capability':'net.wifi','api':1},needs);self.assertIn({'capability':'bluetooth.hci','api':1},needs)
 def test_home_alone_needs_no_controls_or_new_grants(self):
  p,a=self.args('--home-app','default.elf');flags,sources=quick.configure(a,p,ROOT,Path('/unused'))
  self.assertEqual(flags,['-DPORTABLE_HOME_APP="default.elf"']);self.assertFalse(sources)
 def test_helpers_do_not_write_defaults_or_grants(self):
  source=(ROOT/'scripts/portable_quick_build.py').read_text()
  self.assertNotIn('hardware.device',source);self.assertNotIn('create_grant',source)

"""Explicit independent selection and exact effective prefix staging."""
import argparse,contextlib,hashlib,io,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
import portable_paper_build as paper
import portable_quick_build as quick

class PaperTransitionBuild(unittest.TestCase):
 def parser(self):
  parser=argparse.ArgumentParser();paper.options(parser);quick.options(parser);parser.add_argument('--alarm-client',action='store_true');return parser
 def test_flags_are_independent_and_off_has_no_sdk_mutation(self):
  parser=self.parser()
  with tempfile.TemporaryDirectory() as directory:
   out=Path(directory)
   for selected in (False,True):
    args=parser.parse_args(['--quick-actions','--alarm-client']+(['--paper-transitions'] if selected else []))
    flags,_=quick.configure(args,parser,ROOT,out)
    self.assertEqual('-DPORTABLE_PAPER_TRANSITIONS' in flags,selected)
    include,fade_flags,receipt=paper.stage(args,parser,ROOT,out,ROOT/'lib/PortableApps/include')
    self.assertEqual(include,ROOT/'lib/PortableApps/include');self.assertEqual(fade_flags,[]);self.assertIsNone(receipt)
    self.assertFalse((out/'paper-sdk').exists())
 def test_incomplete_or_implicit_selection_fails(self):
  parser=self.parser()
  for flags in (['--paper-crossfade'],['--paper-display-sdk','/missing']):
   with contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):paper.validate(parser.parse_args(flags),parser)
  with tempfile.TemporaryDirectory() as directory:
   out=Path(directory);sdk=out/'input';sdk.mkdir();(sdk/paper.HEADERS[-1]).write_text('suffix only')
   args=parser.parse_args(['--paper-crossfade','--paper-display-sdk',str(sdk)])
   with contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):paper.stage(args,parser,ROOT,out,ROOT/'lib/PortableApps/include')
   self.assertFalse((out/'paper-sdk').exists())
 def test_stages_all_four_headers_and_exact_source_receipt(self):
  parser=self.parser()
  with tempfile.TemporaryDirectory() as directory:
   out=Path(directory);sdk=out/'input';sdk.mkdir()
   for name in paper.HEADERS:(sdk/name).write_text('canonical fixture '+name)
   args=parser.parse_args(['--paper-crossfade','--paper-display-sdk',str(sdk)])
   paper.validate(args,parser)
   before=(ROOT/'lib/PortableApps/include/RiscDisplayOutputV1.h').read_bytes()
   includes,flags,receipt=paper.stage(args,parser,ROOT,out,ROOT/'lib/PortableApps/include')
   self.assertEqual(flags,['-DPORTABLE_PAPER_CROSSFADE']);self.assertNotIn('version',receipt);self.assertNotIn('commit',receipt)
   for name in paper.HEADERS:
    data=(sdk/name).read_bytes();self.assertEqual((includes/name).read_bytes(),data)
    self.assertEqual(receipt['sdk_sha256'][name],hashlib.sha256(data).hexdigest())
   self.assertEqual((ROOT/'lib/PortableApps/include/RiscDisplayOutputV1.h').read_bytes(),before)
   (includes/'stale.h').write_text('old');paper.stage(args,parser,ROOT,out,ROOT/'lib/PortableApps/include');self.assertFalse((includes/'stale.h').exists())
   for path,digest in paper.motion_receipt(ROOT)['source_sha256'].items():self.assertEqual(digest,hashlib.sha256((ROOT/path).read_bytes()).hexdigest())

if __name__=='__main__':unittest.main()

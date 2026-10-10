import argparse
import unittest
from types import SimpleNamespace
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"scripts"))
import portable_file_sharing_build as sharing
class SharingProfile(unittest.TestCase):
 def options(self,argv):
  p=argparse.ArgumentParser();sharing.options(p);a=p.parse_args(argv);sharing.validate(a,p);return a
 def test_default_is_empty(self):
  a=self.options([]);self.assertEqual(sharing.configure(a,None,None,None,None),([],[],[]))
  needs=[{"capability":"display.output","api":1}];sharing.requirements(a,needs);self.assertEqual(len(needs),1)
 def test_explicit_selection(self):
  a=self.options(["--webdav-sharing","--sharing-export-instance","32","--sharing-tcp-instance","33","--sharing-entropy-instance","34"]);needs=[];sharing.requirements(a,needs);sharing.requirements(a,needs);self.assertEqual(len(needs),5)
 def test_reject_incomplete_or_ambiguous(self):
  for argv in (["--webdav-sharing"],["--sharing-export-instance","32"],["--webdav-sharing","--sharing-export-instance","0","--sharing-tcp-instance","33","--sharing-entropy-instance","34"],["--webdav-sharing","--sharing-export-instance","32","--sharing-tcp-instance","32","--sharing-entropy-instance","34"]):
   with self.assertRaises(SystemExit):self.options(argv)
if __name__=="__main__":unittest.main()

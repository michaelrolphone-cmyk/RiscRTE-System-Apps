#!/usr/bin/env python3
import unittest
from inspect_infrastructure_state import diagnose_frame
from provision_infrastructure import frame

class Diagnostic(unittest.TestCase):
 def test_shape_mismatch_without_disclosing_private_values(self):
  state={'private':'never-disclose','relay':{str(i):i for i in range(40)}}
  state['relay']['pending']={str(i):i for i in range(30)}
  report=diagnose_frame(frame(state))
  self.assertFalse(report['legacy_shape_within_bounds'])
  self.assertTrue(report['repaired_shape_within_bounds'])
  self.assertNotIn('never-disclose',str(report))
 def test_corrupt_frame_is_not_repaired_or_decoded(self):
  raw=bytearray(frame({'private':'never-disclose'}));raw[-1]^=1
  self.assertEqual(diagnose_frame(raw)['status'],'frame_or_json_invalid')

if __name__=='__main__':unittest.main()

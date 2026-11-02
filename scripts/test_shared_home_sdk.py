#!/usr/bin/env python3
"""No compiler needed: exact shared header closure, prefix preservation and refusal."""
import tempfile,unittest
from pathlib import Path
from shared_home_sdk import stage_home_headers
class Closure(unittest.TestCase):
 def test_transitive_and_prefix(self):
  with tempfile.TemporaryDirectory() as temp:
   root=Path(temp)/'source';target=Path(temp)/'stage';target.mkdir(parents=True)
   sdk=root/'sdk/app';sdk.mkdir(parents=True)
   portable=root/'lib/PortableApps/include';portable.mkdir(parents=True)
   for name in ('RiscHomePresentationV1.h','RiscResidentContextsV1.h','RiscCivilClockV1.h'):
    (sdk/name).write_text('#pragma once\n')
   (portable/'PortableHomeCanvas.h').write_text('#include "RiscCanvasV1.h"\n#include "RiscDisplayHistoryV1.h"\n')
   (sdk/'RiscCanvasV1.h').write_text('#include "RiscDisplayOutputV1.h"\n')
   (sdk/'RiscDisplayHistoryV1.h').write_text('#include "RiscDisplayOutputV1.h"\n')
   # An owner source must not replace the already selected physical prefix.
   (sdk/'RiscDisplayOutputV1.h').write_text('wrong unselected prefix\n')
   (target/'RiscDisplayOutputV1.h').write_text('exact selected prefix\n')
   receipt=stage_home_headers(root,target)
   self.assertEqual((target/'RiscDisplayOutputV1.h').read_text(),'exact selected prefix\n')
   self.assertEqual((target/'RiscDisplayHistoryV1.h').read_bytes(),(sdk/'RiscDisplayHistoryV1.h').read_bytes())
   self.assertIn('sdk/app/RiscDisplayHistoryV1.h',receipt['copied_source_sha256'])
   self.assertEqual(len(receipt['closure_sha256']),7)
   # Generalized closure: a future nested owner SDK extension is also staged.
   (sdk/'RiscDisplayHistoryV1.h').write_text('#include "NestedV1.h"\n')
   (sdk/'NestedV1.h').write_text('#pragma once\n')
   (target/'RiscDisplayHistoryV1.h').unlink()
   stage_home_headers(root,target)
   self.assertEqual((target/'NestedV1.h').read_bytes(),(sdk/'NestedV1.h').read_bytes())
   (sdk/'RiscCanvasV1.h').write_text('#include "MissingV1.h"\n')
   with self.assertRaisesRegex(ValueError,'Missing shared Home header MissingV1.h'):
    stage_home_headers(root,target)
if __name__=='__main__':unittest.main()

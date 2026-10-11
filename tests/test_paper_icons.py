import json,re,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]/'lib/PortableApps'
class PaperIcons(unittest.TestCase):
 def test_real_marker_and_control_coverage(self):
  expected={'solid:f111':'circle','regular:f111':'circle','solid:f0c8':'square','solid:f0d8':'caret-up','solid:f219':'diamond','solid:f067':'plus','solid:f068':'minus','solid:f7a5':'grip-lines-vertical','solid:f054':'chevron-right','solid:f053':'chevron-left','solid:f120':'terminal'}
  additions=json.loads((ROOT/'additional-icons.json').read_text());provenance=json.loads((ROOT/'fonts/SOURCES.json').read_text());source=(ROOT/'fonts/icons.inc').read_text()
  for name,glyph in expected.items():
   self.assertEqual(additions[name],glyph)
   self.assertEqual(provenance['glyph_names'][name],glyph)
   self.assertIn(name,provenance['icons'])
   self.assertRegex(source,r'\{"'+re.escape(name)+r'",[1-9][0-9]*,[1-9][0-9]*,')

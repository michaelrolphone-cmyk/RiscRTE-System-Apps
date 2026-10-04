import hashlib
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]

class PortableIconSubset(unittest.TestCase):
    def test_alarm_and_countdown_glyphs_are_embedded_and_named(self):
        provenance=json.loads((ROOT/'lib/PortableApps/fonts/SOURCES.json').read_text())
        self.assertEqual(provenance['glyph_names']['solid:f0f3'],'bell')
        self.assertEqual(provenance['glyph_names']['solid:f254'],'hourglass')
        self.assertIn('solid:f0f3',provenance['icons'])
        self.assertIn('solid:f254',provenance['icons'])
        data=(ROOT/'lib/PortableApps/fonts/icons.inc').read_bytes()
        self.assertEqual(hashlib.sha256(data).hexdigest(),provenance['icons.inc_sha256'])
        text=data.decode()
        self.assertIn('{"solid:f0f3",33,38,rpi_solid_f0f3}',text)
        self.assertIn('{"solid:f254",29,38,rpi_solid_f254}',text)

    def test_alarm_and_countdown_icons_are_distinct(self):
        provenance=json.loads((ROOT/'lib/PortableApps/fonts/SOURCES.json').read_text())
        self.assertNotEqual('solid:f0f3','solid:f254')
        self.assertEqual(len([x for x in provenance['icons'] if x in ('solid:f0f3','solid:f254')]),2)

if __name__=='__main__':
    unittest.main()

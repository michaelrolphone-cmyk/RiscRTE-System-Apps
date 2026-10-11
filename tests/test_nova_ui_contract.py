import json,re,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class NovaContract(unittest.TestCase):
 def test_catalog_has_real_unique_glyphs(self):
  root=ROOT/'lib/PortableApps'
  apps=json.loads((root/'catalog-icons.json').read_text())['apps']
  icons=json.loads((root/'fonts/SOURCES.json').read_text())
  self.assertEqual(len(apps),len({v['icon'] for v in apps.values()}))
  raster=(root/'fonts/icons.inc').read_text()
  for app,v in apps.items():
   self.assertIn(v['icon'],icons['icons'],app)
   self.assertEqual(v['glyph'],icons['glyph_names'][v['icon']],app)
   self.assertRegex(raster,r'\{"'+re.escape(v['icon'])+r'",[1-9][0-9]*,[1-9][0-9]*,',app)
 def test_shared_theme_uses_settings_fonts(self):
  source=(ROOT/'lib/PortableApps/src/nova_ui.inc').read_text()
  self.assertIn('../settings_fonts/text.inc',source)
  for token in ('0x19e3ffu','0x0e4f5cu','0x12262bu','0x6b8288u','0xcfe9eeu'):
   self.assertIn(token,(ROOT/'lib/PortableApps/include/PortableNovaUi.h').read_text())
if __name__=='__main__':unittest.main()

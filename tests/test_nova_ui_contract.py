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
 def test_touchpad_uses_audited_arrow_pointer(self):
  root=ROOT/'lib/PortableApps'
  pins=json.loads((root/'fonts/SOURCES.json').read_text())
  self.assertEqual(json.loads((root/'additional-icons.json').read_text())['solid:f245'],'arrow-pointer')
  self.assertEqual(pins['glyph_names']['solid:f245'],'arrow-pointer')
  self.assertIn('solid:f245',pins['icons'])
  self.assertRegex((root/'fonts/icons.inc').read_text(),r'\{"solid:f245",[1-9][0-9]*,[1-9][0-9]*,rpi_solid_f245\}')
if __name__=='__main__':unittest.main()

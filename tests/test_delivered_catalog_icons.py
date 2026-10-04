import json,re,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class DeliveredIcons(unittest.TestCase):
 def test_actual_nine_app_inventory_is_complete_and_unique(self):
  catalog=json.loads((ROOT/'tests/fixtures/nova-delivered-catalog.json').read_text())
  registry=json.loads((ROOT/'lib/PortableApps/catalog-icons.json').read_text())['apps']
  raster=(ROOT/'lib/PortableApps/fonts/icons.inc').read_text()
  self.assertEqual(len(catalog),9)
  self.assertEqual({a['file_name'].split('.')[0] for a in catalog},set(registry))
  self.assertEqual(len(catalog),len({a['icon'] for a in catalog}))
  for a in catalog:
   self.assertEqual(a['icon'],registry[a['file_name'].split('.')[0]]['icon'])
   self.assertRegex(raster,r'\{"'+re.escape(a['icon'])+r'",[1-9][0-9]*,[1-9][0-9]*,')
if __name__=='__main__':unittest.main()

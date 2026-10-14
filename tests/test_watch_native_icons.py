import json,re,subprocess,sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class NativeIcons(unittest.TestCase):
 def test_complete_watch_inventory_and_reproducible_rasters(self):
  file=ROOT/'lib/PortableApps/fonts/watch-native-icons.inc';before=file.read_bytes()
  subprocess.run([sys.executable,str(ROOT/'scripts/generate_watch_native_icons.py')],check=True)
  self.assertEqual(before,file.read_bytes())
  icons={m[0]:tuple(map(int,m[1:])) for m in re.findall(r'\{"([^"]+)",(\d+),(\d+),',before.decode())}
  rows=json.loads((ROOT/'tests/fixtures/watch-suite-catalog.json').read_text())
  self.assertEqual(len(rows),23)
  for row in rows:
   self.assertIn(row['icon'],icons,row['file_name'])
   self.assertGreaterEqual(max(icons[row['icon']]),28,row['file_name'])
  files=json.loads((ROOT/'Apps/native/file_browser.json').read_text())
  self.assertEqual(files['icon'],'solid:f07c');self.assertEqual(files['display_name'],'Files')
if __name__=='__main__':unittest.main()

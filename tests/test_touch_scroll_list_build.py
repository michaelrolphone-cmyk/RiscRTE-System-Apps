"""List scrolling is an explicit native-paper selection, never a Watch default."""
import subprocess
import sys
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class TouchScrollListBuild(unittest.TestCase):
    def test_default_profiles_reject_scrolling(self):
        for app in ('wifi','file_browser','springboard'):
            with self.subTest(app=app):
                result=subprocess.run([sys.executable,str(ROOT/'scripts'/f'build_portable_{app}.py'),'--touch-scrolling'],capture_output=True,text=True)
                self.assertEqual(result.returncode,2)
                self.assertIn('--touch-scrolling requires --time-profile x4-native-time and --paper-transitions',result.stderr)
if __name__=='__main__':unittest.main()

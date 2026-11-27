import json
import subprocess
import sys
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class WatchFilesBuildProfile(unittest.TestCase):
    def test_explicit_version_and_no_alarm(self):
        value=json.loads((ROOT/'lib/PortableApps/profiles/watch-native-time-file-browser.json').read_text())
        self.assertEqual(value['profile'],'watch-native-time')
        self.assertIsNone(value['alarm_api'])
        self.assertEqual(value['version'],'1.6.0')
    def test_rejects_misaligned_routes_before_compiler(self):
        for options in (['--display-rotation','180'],['--touch-scrolling'],['--paper-transitions']):
            proc=subprocess.run([sys.executable,str(ROOT/'scripts/build_portable_file_browser.py'),'--watch-native-ui',*options],text=True,capture_output=True)
            self.assertNotEqual(proc.returncode,0)
            self.assertIn('Watch native Files requires rotation zero and non-paper input',proc.stderr)
    def test_exposes_both_home_and_foreground_options(self):
        proc=subprocess.run([sys.executable,str(ROOT/'scripts/build_portable_file_browser.py'),'--help'],text=True,capture_output=True,check=True)
        for flag in ['--home-app','--return-app','--foreground-idle-helper-dir','--native-time-runtime-repo','--watch-native-ui']:
            self.assertIn(flag,proc.stdout)

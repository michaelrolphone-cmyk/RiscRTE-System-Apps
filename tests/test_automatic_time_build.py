"""Explicit source selection and SDK-copy custody; no network or target hardware."""
import argparse
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('build_installed_apps', ROOT / 'scripts/build_installed_apps.py')
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)
RUNTIME = Path(os.environ['RUNTIME_REPO']) if os.environ.get('RUNTIME_REPO') else None


class AutomaticTimeBuild(unittest.TestCase):
    def test_no_implicit_time_server(self):
        help_text = subprocess.check_output([sys.executable, str(ROOT / 'scripts/build_installed_apps.py'), '--help'], text=True)
        self.assertIn('--time-server TIME_SERVER', help_text)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for flags in (['--time-server', 'time.cloudflare.com'],
                          ['--time-server', 'https://time.cloudflare.com', '--boot-profiles', '--native-feed'],
                          ['--time-server', 'time.cloudflare.com', '--boot-profiles']):
                result = subprocess.run([sys.executable, str(ROOT / 'scripts/build_installed_apps.py'),
                                         '--runtime-repo', str(root), '--output-dir', str(root / 'out'), *flags],
                                        capture_output=True, text=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse((root / 'out').exists())

    def test_explicit_hostname_is_generic_and_bounded(self):
        for server in ('time.cloudflare.com', 'time.example', 'NTP-1.example', 'localhost',
                       '.'.join(['a' * 63] * 3 + ['a' * 61])):
            self.assertEqual(builder.time_server(server), server)
        for server in ('', '.example', 'time..example', 'time.example.', '-time.example',
                       'time-.example', 'https://time.cloudflare.com', 'a' * 64 + '.example',
                       '.'.join(['a' * 63] * 4), 'time.example:123', 'time_1.example',
                       'time.example\n', 'tíme.example', 'time.example"'):
            with self.subTest(server=server), self.assertRaises(argparse.ArgumentTypeError):
                builder.time_server(server)

    def test_mismatched_sdk_copy_fails_before_compilation(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'sdk/driver').mkdir(parents=True)
            (root / 'sdk/driver/RiscHttpTimeV1.h').write_text('not the selected SDK contract\n')
            result = subprocess.run([sys.executable, str(ROOT / 'scripts/build_installed_apps.py'),
                                     '--runtime-repo', str(root), '--output-dir', str(root / 'out'),
                                     '--native-feed', '--boot-profiles', '--time-server', 'time.cloudflare.com'],
                                    capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('Time suffix must match', result.stderr)
            self.assertFalse((root / 'out').exists())

    @unittest.skipUnless(RUNTIME, 'Set RUNTIME_REPO to verify canonical SDK headers')
    def test_time_and_base_headers_match_selected_runtime(self):
        for name in ('RiscHttpClientV1.h', 'RiscHttpTimeV1.h'):
            self.assertEqual((ROOT / 'lib/PortableApps/include' / name).read_bytes(),
                             (RUNTIME / 'sdk/driver' / name).read_bytes(), name)


if __name__ == '__main__':
    unittest.main()

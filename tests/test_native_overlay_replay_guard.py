"""Frozen replay tokens prove the new COLOR_ONLY guard leaves other paths exact."""
import hashlib
import os
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'docs/native-overlays/replay-baseline-b8e34df8.inc'
CURRENT = ROOT / 'lib/PortableApps/src/raster_snapshot_replay.inc'
BASE_SHA = 'b5e22a8eb81065f535ff25eeadf66c52b10a87cdb7ab79da0fe53d6c0d8ce7bd'


def tokens(path, defines):
    return subprocess.check_output([os.environ.get('CC', 'cc'), '-E', '-P', '-x', 'c',
                                   '-I' + str(ROOT / 'lib/PortableApps/src'),
                                   *('-D' + d for d in defines), str(path)], text=True)


class ReplayGuard(unittest.TestCase):
    def test_frozen_original(self):
        self.assertEqual(hashlib.sha256(BASE.read_bytes()).hexdigest(), BASE_SHA)

    def test_paper_and_resident_tokens_are_unchanged(self):
        for quick in (False, True):
            for resident in (False, True):
                for native in (False, True):
                    flags = [name for enabled, name in (
                        (quick, 'PORTABLE_QUICK_ACTIONS'),
                        (resident, 'PORTABLE_RESIDENT_SHELL_HOST'),
                        (native, 'PORTABLE_QUICK_NATIVE')) if enabled]
                    with self.subTest(flags=flags):
                        self.assertEqual(tokens(BASE, flags), tokens(CURRENT, flags))

    def test_color_only_preserves_rgb_replay_and_omits_paper_dispatch(self):
        for resident in (False, True):
            for native in (False, True):
                flags = ['PORTABLE_QUICK_ACTIONS', 'PORTABLE_WATCH_COLOR_ONLY']
                if resident:
                    flags.append('PORTABLE_RESIDENT_SHELL_HOST')
                if native:
                    flags.append('PORTABLE_QUICK_NATIVE')
                with self.subTest(flags=flags):
                    old, new = tokens(BASE, flags), tokens(CURRENT, flags)
                    rgb = lambda text: text.split('case RS_QA_RENDER:', 1)[1].split('break;', 1)[0]
                    self.assertEqual(rgb(old), rgb(new))
                    for name in ('RS_QP_ICON:', 'RS_QR_TEXT:', 'RS_QR_ROUND:', 'RS_QR_ICON:',
                                 'qp_icon(', 'qr_text(', 'qr_round(', 'qr_icon('):
                        self.assertNotIn(name, new)
                    self.assertIn('case RS_QR_TEXT:' if resident else 'case RS_QP_ICON:', old)


if __name__ == '__main__':
    unittest.main()

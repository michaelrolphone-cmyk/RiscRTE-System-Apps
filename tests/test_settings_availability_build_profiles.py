import contextlib
import io
import sys
import unittest
from pathlib import Path
from unittest import mock
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
import build_portable_settings as settings
import build_paper_clock as home

class SettingsAvailabilitySelection(unittest.TestCase):
    def test_settings_default_off(self):
        self.assertFalse(settings.argument_parser().parse_args([]).experimental_settings_availability)

    def test_settings_rejects_without_x4_atomic(self):
        for args in ([],['--settings-profile','x4-native-time'],['--experimental-atomic-settings']):
            with self.subTest(args=args),contextlib.redirect_stderr(io.StringIO()),mock.patch.object(settings.subprocess,'run') as run:
                parser=settings.argument_parser()
                with self.assertRaises(SystemExit):settings.build(parser.parse_args(['--experimental-settings-availability',*args]),parser)
                run.assert_not_called()

    def test_home_rejects_without_atomic_automatic(self):
        for args in ([],['--experimental-atomic-quick'],['--experimental-atomic-home-automatic']):
            with self.subTest(args=args),contextlib.redirect_stderr(io.StringIO()),mock.patch.object(home.subprocess,'run') as run,mock.patch.object(sys,'argv',['build_paper_clock.py','--experimental-settings-availability',*args]):
                with self.assertRaises(SystemExit):home.build()
                run.assert_not_called()

    def test_runtime_status_header_exact(self):
        root=Path(__file__).resolve().parents[1]
        header=(root/'lib/PortableApps/include/RiscKeyValueAvailabilityV1.h').read_text()
        self.assertIn('RISC_KEY_VALUE_RECONCILIATION_PENDING=2',header)
        self.assertIn('storage.key-value.availability',header)
        self.assertNotIn('put)',header)

if __name__=='__main__':unittest.main()

import contextlib
import io
from pathlib import Path
import sys
import unittest
from unittest import mock
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
import build_portable_settings as settings

class AtomicSettingsSelection(unittest.TestCase):
    def test_default_off(self):
        self.assertFalse(settings.argument_parser().parse_args([]).experimental_atomic_settings)

    def test_reject_incomplete_profile_before_compile(self):
        profiles=[[],['--settings-profile','x4-native-time'],
                  ['--settings-profile','x4-native-time','--resident-shell-client'],
                  ['--settings-profile','x4-native-time','--resident-shell-client','--resident-policy'],
                  ['--settings-profile','x4-native-time','--resident-shell-client','--resident-policy',
                   '--home-desk-lock','--alarm-settings']]
        for profile in profiles:
            with self.subTest(profile=profile),contextlib.redirect_stderr(io.StringIO()),mock.patch.object(settings.subprocess,'run') as run:
                parser=settings.argument_parser();args=parser.parse_args(['--experimental-atomic-settings',*profile])
                with self.assertRaises(SystemExit):settings.build(args,parser)
                run.assert_not_called()

if __name__=='__main__':unittest.main()

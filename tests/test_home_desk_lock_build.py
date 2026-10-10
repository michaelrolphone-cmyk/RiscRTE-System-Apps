"""Home desk lock is an explicit selection on the combined native paper profile."""
import contextlib
import io
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
import build_portable_settings as settings

class HomeDeskLockBuild(unittest.TestCase):
    def test_default_unselected(self):
        self.assertFalse(settings.argument_parser().parse_args([]).home_desk_lock)

    def test_rejects_incomplete_profiles_before_output(self):
        for selection in ([],['--settings-profile','x4-desk-clock'],
                          ['--settings-profile','x4-native-time'],
                          ['--settings-profile','x4-native-time','--paper-transitions'],
                          ['--settings-profile','x4-native-time','--touch-scrolling']):
            with self.subTest(selection=selection),tempfile.TemporaryDirectory() as tmp:
                parser=settings.argument_parser();out=Path(tmp)/'output'
                args=parser.parse_args(['--home-desk-lock',*selection,'--output-dir',str(out)])
                with contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):
                    settings.build(args,parser)
                self.assertFalse(out.exists())

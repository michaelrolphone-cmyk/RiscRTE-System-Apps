"""Scrolling is a distinct, explicit native Settings selection."""
import contextlib
import io
import sys
import tempfile
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
import build_portable_settings as settings

class TouchScrollBuild(unittest.TestCase):
    def test_default_is_unselected(self):
        args=settings.argument_parser().parse_args([])
        self.assertFalse(args.touch_scrolling)
        self.assertFalse(args.settings_list_scrolling)

    def test_rejects_incompatible_profiles_before_creating_output(self):
        for profile in ['default','x4-desk-clock']:
            with self.subTest(profile=profile), tempfile.TemporaryDirectory() as tmp:
                parser=settings.argument_parser()
                out=Path(tmp)/'output'
                args=parser.parse_args(['--touch-scrolling','--paper-transitions',
                    '--settings-profile',profile,'--output-dir',str(out)])
                with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                    settings.build(args,parser)
                self.assertFalse(out.exists())

    def test_native_profile_requires_motion_selection(self):
        parser=settings.argument_parser()
        args=parser.parse_args(['--touch-scrolling','--settings-profile','x4-native-time'])
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            settings.build(args,parser)

    def test_menu_lists_require_existing_native_scroll_profile(self):
        for arguments in [[], ['--settings-profile','x4-native-time'],
                          ['--touch-scrolling','--paper-transitions']]:
            with self.subTest(arguments=arguments), tempfile.TemporaryDirectory() as tmp:
                parser=settings.argument_parser();out=Path(tmp)/'uncreated'
                args=parser.parse_args(['--settings-list-scrolling','--output-dir',str(out),*arguments])
                with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                    settings.build(args,parser)
                self.assertFalse(out.exists())

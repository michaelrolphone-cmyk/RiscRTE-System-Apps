import argparse
import sys
import unittest
import tempfile
from unittest.mock import patch
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
import portable_native_toolbar_build as native

class WatchNativeToolbarProfile(unittest.TestCase):
    def arguments(self, **changes):
        result=dict(time_profile='watch-native-time',display_rotation=None,watch_native_ui=True,
                    native_time_runtime_repo=Path('/runtime'),tagged_alarm_utilities=None,
                    alarm_client=False,navigation=False)
        result.update(changes)
        return argparse.Namespace(**result)

    def test_readonly_no_alarm(self):
        args=self.arguments();native.validate(args,argparse.ArgumentParser())
        self.assertEqual(args.display_rotation,0);self.assertTrue(args.navigation)
        needs=[];native.requirements(args,needs)
        self.assertEqual(needs,[{'capability':name,'api':1} for name in
                               ('runtime.realtime','storage.key-value','board.battery')])

    def test_watch_explicit_geometry(self):
        for changes in [dict(watch_native_ui=False),dict(display_rotation=90),dict(native_time_runtime_repo=None),
                        dict(paper_transitions=True),dict(paper_crossfade=True),dict(resident_shell_host=True),
                        dict(resident_shell_client=True),dict(wall_time=True),dict(denver=True),
                        dict(alarm_client=True),dict(tagged_alarm_utilities=Path('/utilities'))]:
            with self.subTest(changes=changes), self.assertRaises(SystemExit):
                native.validate(self.arguments(**changes),argparse.ArgumentParser())

    def test_optional_alarm_is_explicit_api2(self):
        args=self.arguments(alarm_client=True,tagged_alarm_utilities=Path('/utilities'))
        native.validate(args,argparse.ArgumentParser());needs=[];native.requirements(args,needs)
        self.assertIn({'capability':'alarm.service','api':2},needs)

    def test_foreground_native_receipt_uses_compiled_canonical_sdk(self):
        args=self.arguments(foreground_idle_helper_dir=Path('/helpers'),
                            foreground_idle_runtime_dir=Path('/runtime'))
        sdk={name:b'canonical foreground header' for name in (*native.SDK_HEADERS,'LICENSE')}
        root=Path(__file__).resolve().parents[1]
        with tempfile.TemporaryDirectory() as directory, patch.object(
                native.portable_foreground_idle_build,'runtime_app_sdk',return_value=sdk):
            includes,flags,sources,receipt=native.configure(args,argparse.ArgumentParser(),root,Path(directory),'springboard')
            self.assertEqual(receipt['native_time_sdk']['commit'],native.portable_foreground_idle_build.RUNTIME_COMMIT)
            for name in native.SDK_HEADERS:self.assertEqual((includes/name).read_bytes(),sdk[name])
            self.assertIn('-DPORTABLE_NATIVE_TIME_TOOLBAR',flags)
            self.assertIsNone(receipt['tagged_alarm_sdk'])

    def test_paper_profile_preserved(self):
        args=self.arguments(time_profile='x4-native-time',watch_native_ui=False,alarm_client=True,
                            tagged_alarm_utilities=Path('/utilities'))
        native.validate(args,argparse.ArgumentParser());self.assertEqual(args.display_rotation,90)
        needs=[];native.requirements(args,needs)
        self.assertEqual(needs[0],{'capability':'alarm.service','api':2})

if __name__=='__main__':unittest.main()

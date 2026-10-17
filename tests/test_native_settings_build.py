"""Native presentation build wiring; mocked compilation is not target proof."""
import contextlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'scripts'))
import build_portable_settings as builder
import build_native_settings_qualification as qualification
import publish_native_settings as publisher


class NativeSettingsBuild(unittest.TestCase):
    def test_selection_is_default_off(self):
        args = builder.argument_parser().parse_args([])
        self.assertFalse(args.native_ui)
        self.assertFalse(args.clock_change_control)
        self.assertFalse(args.nova_ui)
        self.assertEqual(args.settings_profile, 'default')

    def test_incompatible_profiles_and_clock_change_fail_before_compile(self):
        cases = [
            (('--native-ui', '--settings-profile', profile), '--native-ui requires')
            for profile in ('x4-desk-clock', 'x4-native-time')]
        cases += [
            (('--clock-change-control',), '--clock-change-control requires --alarm-client'),
            (('--clock-change-control', '--alarm-client', '--settings-profile', 'x4-native-time'),
             '--clock-change-control requires the raw RTC Settings controller')]
        for flags, message in cases:
            with self.subTest(flags=flags), mock.patch.object(builder, 'resolve_compiler') as compiler:
                parser = builder.argument_parser()
                error = io.StringIO()
                with contextlib.redirect_stderr(error), self.assertRaises(SystemExit):
                    builder.build(parser.parse_args(flags), parser)
                self.assertIn(message, error.getvalue())
                compiler.assert_not_called()

    def test_explicit_complete_profile_declares_generic_retained_capabilities(self):
        profile = qualification.manifest('1.4.2')
        self.assertEqual(profile, json.loads((ROOT/qualification.MANIFEST_PROFILE).read_text()))
        with self.assertRaisesRegex(ValueError, 'source manifest differs'):
            qualification.manifest('1.3.4')
        with self.assertRaisesRegex(ValueError, 'source manifest differs'):
            qualification.manifest('1.4.0')
        self.assertEqual(profile['version'], '1.4.2')
        self.assertEqual(len(profile['requires']), 16)
        self.assertEqual({item['capability'] for item in profile['requires']}, {
            'display.output', 'input.touch.raw', 'rtc.clock', 'alarm.service',
            'telemetry.broadcast', 'contexts.service', 'input.power-key',
            'power.sleep', 'display.power', 'board.battery', 'storage.key-value',
            'net.wifi', 'bluetooth.hci', 'motion.accel', 'app.catalog',
            'storage.key-value.atomic'})
        for flag in ('PORTABLE_QUICK_NATIVE', 'PORTABLE_ALARM_NATIVE', 'PORTABLE_SETTINGS_NATIVE_UI', 'PORTABLE_NATIVE_CANVAS',
                     'PORTABLE_SETTINGS_ATOMIC_EXPERIMENTAL', 'PORTABLE_QUICK_ATOMIC_PREFERENCES'):
            self.assertIn(flag, qualification.DEFINES)

    def test_complete_recipe_refuses_subset_or_inconsistent_version(self):
        arguments = ['-D'+name for name in qualification.REQUIRED_FEATURES]
        arguments += ['-DPORTABLE_SETTINGS_VERSION="1.4.2"']
        arguments += ['source/'+name for name in ('settings.c', 'adapter.c',
            'portable_navigation.c', 'portable_sleep.c', 'quick_actions.c',
            'quick_render.c', 'quick_session.c', 'quick_radios.c')]
        qualification.validate_recipe(arguments, '1.4.2')
        self.assertEqual(set(qualification.CAPABILITY_SELECTORS),
                         {name for name, version in qualification.CAPABILITIES})
        for missing in ('PORTABLE_QUICK_NATIVE', 'PORTABLE_ALARM_NATIVE', 'PORTABLE_BLE_BROADCAST', 'PORTABLE_RUNTIME_APP_CATALOG',
                        'PORTABLE_SETTINGS_ATOMIC_EXPERIMENTAL', 'PORTABLE_MOTION_WAKE'):
            with self.subTest(missing=missing), self.assertRaisesRegex(ValueError, 'lacks'):
                qualification.validate_recipe([x for x in arguments if x != '-D'+missing], '1.4.2')
        with self.assertRaisesRegex(ValueError, 'version'):
            qualification.validate_recipe(arguments, '1.3.4')
        with self.assertRaisesRegex(ValueError, 'source selection'):
            qualification.validate_recipe([x for x in arguments if not x.endswith('portable_sleep.c')], '1.4.2')

    def test_selected_profile_publisher_and_workflow_versions_match(self):
        version = json.loads((ROOT/qualification.VERSION_PROFILE).read_text())['version']
        self.assertEqual(version, '1.4.2')
        self.assertEqual(publisher.VERSION, version)
        self.assertEqual(publisher.TAG, 'native-settings-'+version+'-dev1')
        self.assertNotIn('native-settings-1.4.0-dev1.zip', publisher.REQUIRED)
        workflow = (ROOT/'.github/workflows/native-settings.yml').read_text()
        self.assertIn("r['version']=='"+version+"'", workflow)
        self.assertIn("r['bundle']=='"+publisher.TAG+"'", workflow)
        self.assertEqual(json.loads((ROOT/'Apps/settings.json').read_text())['version'], '1.3.4')

    def test_external_recipe_reads_immutable_objects_only(self):
        with mock.patch.object(qualification.subprocess, 'check_output', return_value=b'canonical') as read:
            self.assertEqual(qualification.pinned({'bridges': Path('/bridge')},
                             'bridges', 'apps/clock/portable_sleep.c'), b'canonical')
        read.assert_called_once_with(['git', '-C', '/bridge', 'show',
            qualification.PINS['bridges']+':apps/clock/portable_sleep.c'])

    def test_native_flags_receipt_features_and_legacy_identity(self):
        # Symlink read-only inputs instead of copying the source/font tree.
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in ('Apps', 'lib', 'scripts', 'LICENSE'):
                (root/name).symlink_to(ROOT/name, target_is_directory=(ROOT/name).is_dir())
            elf = b'\x7fELF\x01\x01\x01'+b'\0'*9+b'\x03\x00\x5e\x00'
            commands = []

            def run(command, **kwargs):
                if '-shared' in command:
                    commands.append(command)
                    Path(command[-1]).write_bytes(elf)

            def output(command, **kwargs):
                if command[0].endswith('nm'):
                    return '0000 T app_main\n0001 T app_module_init\n0002 T app_module_fini\n U risc_runtime_get_api\n'
                if command[-1] == '--version': return 'compiler fixture only\n'
                if command == ['git', 'rev-parse', 'HEAD']: return 'fixture-commit\n'
                if command == ['git', 'status', '--porcelain']: return ''
                raise AssertionError(command)

            cases = (
                ('default', ()),
                ('legacy-nova', ('--nova-ui',)),
                ('native', ('--native-ui',)),
                ('native-explicit-nova', ('--native-ui', '--nova-ui')),
                ('native-full', ('--native-ui', '--clock-change-control', '--alarm-client',
                 '--alarm-settings', '--sleep-settings', '--navigation', '--quick-actions',
                 '--quick-radios', '--paper-transitions', '--home-app', 'springboard.elf')))
            with mock.patch.object(builder, 'ROOT', root), \
                    mock.patch.object(builder, 'resolve_compiler', return_value='xtensa-esp32s3-elf-gcc'), \
                    mock.patch.object(builder.subprocess, 'run', side_effect=run), \
                    mock.patch.object(builder.subprocess, 'check_output', side_effect=output), \
                    mock.patch.object(builder.portable_quick_build, 'version',
                        side_effect=lambda args, app, current: '1.3.28' if args.native_ui and args.nova_ui else current), \
                    contextlib.redirect_stdout(io.StringIO()):
                for name, flags in cases:
                    parser = builder.argument_parser()
                    builder.build(parser.parse_args([*flags, '--output-dir', str(root/name)]), parser)
            version = json.loads((ROOT/'Apps/settings.json').read_text())['version']
            native_version = json.loads((ROOT/'lib/PortableApps/profiles/native-settings.json').read_text())['version']
            self.assertEqual(native_version, '1.4.2')
            self.assertNotEqual(native_version, version)
            for (name, flags), command in zip(cases, commands):
                with self.subTest(name=name):
                    selected = '--native-ui' in flags
                    record = json.loads((root/name/'settings-build-record.json').read_text())
                    manifest = json.loads((root/name/'settings.json').read_text())
                    selected_version = native_version if selected else version
                    self.assertEqual(record['version'], selected_version)
                    self.assertEqual(manifest['version'], selected_version)
                    self.assertIn('-DPORTABLE_SETTINGS_VERSION="'+selected_version+'"', command)
                    self.assertIn('-DPORTABLE_SETTINGS_VERSION="'+selected_version+'"', record['build_defines'])
                    self.assertIn('lib/PortableApps/profiles/native-settings.json' if selected else 'Apps/settings.json', record['source_sha256'])
                    self.assertEqual(record['native_ui'], selected)
                    self.assertEqual('native_ui_contract' in record, selected)
                    self.assertEqual(record['clock_change_control'], name == 'native-full')
                    self.assertEqual(command.count('-DPORTABLE_NOVA_UI'), int(selected or '--nova-ui' in flags))
                    for define in ('PORTABLE_SETTINGS_NATIVE_UI', 'PORTABLE_NATIVE_CANVAS'):
                        self.assertEqual('-D'+define in command, selected)
                        self.assertEqual('-D'+define in record['build_defines'], selected)
                    self.assertEqual(record['settings_profile'], 'native-settings' if selected else 'default')
                    self.assertEqual(record['display_rotation'], 0)
                    self.assertEqual(record['purpose'], 'portable-development-artifact-not-deployment')
                    if name != 'native-full': continue
                    for define in ('PORTABLE_SETTINGS_CLOCK_CHANGE_CONTROL', 'PORTABLE_ALARM_CLIENT',
                                   'PORTABLE_ALARM_SETTINGS', 'PORTABLE_SLEEP_SETTINGS',
                                   'PORTABLE_INPUT_NAVIGATION', 'PORTABLE_QUICK_ACTIONS',
                                   'PORTABLE_QUICK_RADIOS', 'PORTABLE_PAPER_TRANSITIONS'):
                        self.assertIn('-D'+define, record['build_defines'])
                    self.assertEqual(record['sleep_modes'], ['light', 'deep', 'hybrid'])
                    self.assertTrue(record['quick_actions'])
                    self.assertTrue(record['quick_radios'])
                    self.assertEqual({item['capability'] for item in manifest['requires']},
                                     {'display.output', 'input.touch.raw', 'rtc.clock', 'alarm.service',
                                      'storage.key-value', 'input.navigation', 'board.battery',
                                      'net.wifi', 'bluetooth.hci'})
                    for source in ('quick_actions.c', 'quick_render.c', 'quick_session.c', 'quick_radios.c'):
                        self.assertIn(str(root/'lib/PortableApps/src'/source), command)
                    self.assertIn('lib/PortableApps/include/AlarmClockChangeV1.h', record['source_sha256'])


if __name__ == '__main__':
    unittest.main()

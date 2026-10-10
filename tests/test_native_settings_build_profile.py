"""Builder contract checks; mocked compiler calls do not prove target behavior."""
import contextlib
import hashlib
import io
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'scripts'))
import build_portable_settings as builder


class NativeSettingsBuildProfile(unittest.TestCase):
    def args(self, *flags):
        parser = builder.argument_parser()
        return parser, parser.parse_args(flags)

    def test_profile_rejects_incompatible_or_missing_inputs_before_compile(self):
        cases = [('--settings-profile', 'x4-native-time'),
                 ('--settings-profile', 'x4-native-time', '--display-rotation', '0'),
                 ('--settings-profile', 'x4-native-time', '--nova-ui'),
                 ('--settings-profile', 'x4-native-time', '--denver'),
                 ('--settings-profile', 'x4-native-time', '--wall-time'),
                 ('--native-time-runtime-repo', '/unused'),
                 ('--settings-profile', 'x4-desk-clock', '--native-time-runtime-repo', '/unused')]
        for flags in cases:
            with self.subTest(flags=flags), mock.patch.object(builder.subprocess, 'run') as run:
                parser, args = self.args(*flags)
                with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                    builder.build(args, parser)
                run.assert_not_called()

    def test_runtime_headers_and_license_use_only_pinned_git_objects(self):
        parser, args = self.args('--native-time-runtime-repo', '/runtime')
        with mock.patch.object(builder.subprocess, 'check_output', return_value=b'canonical') as read:
            sdk = builder.native_time_sdk(args, parser)
        self.assertEqual(set(sdk), {'RiscRuntimeV1.h', 'RiscRealtimeV1.h', 'LICENSE'})
        self.assertEqual([call.args[0] for call in read.call_args_list], [
            ['git', '-C', '/runtime', 'show', builder.NATIVE_TIME_RUNTIME_COMMIT+':'+path]
            for path in ('sdk/app/RiscRuntimeV1.h', 'sdk/app/RiscRealtimeV1.h', 'LICENSE')])
        with mock.patch.object(builder.subprocess, 'check_output',
                               side_effect=subprocess.CalledProcessError(128, ['git'])):
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                builder.native_time_sdk(args, parser)

    def test_staging_keeps_relative_includes_and_replaces_the_old_prefix(self):
        sdk = {name: ('canonical '+name).encode() for name in builder.NATIVE_TIME_SDK_HEADERS}
        with tempfile.TemporaryDirectory() as directory:
            includes = builder.stage_native_time_sdk(Path(directory), sdk)
            for path in (ROOT/'lib/PortableApps/include').rglob('*'):
                if path.is_file():
                    relative = path.relative_to(ROOT/'lib/PortableApps/include')
                    self.assertEqual((includes/relative).read_bytes(), sdk.get(path.name, path.read_bytes()))
            self.assertEqual((includes.parent/'time/denver/display_time.h').read_bytes(),
                             (ROOT/'lib/PortableApps/time/denver/display_time.h').read_bytes())
            self.assertFalse((includes/'RiscProviderPromotionV1.h').exists())

    def test_missing_controller_cannot_be_reported_as_native_time_build(self):
        parser, args = self.args('--settings-profile', 'x4-native-time')
        with tempfile.TemporaryDirectory() as directory, mock.patch.object(builder, 'ROOT', Path(directory)), \
                mock.patch.object(builder, 'native_time_sdk', return_value={}), \
                mock.patch.object(builder.subprocess, 'run') as run:
            errors = io.StringIO()
            with contextlib.redirect_stderr(errors), self.assertRaises(SystemExit):
                builder.build(args, parser)
            self.assertIn('Missing native-time Settings implementation', errors.getvalue())
            run.assert_not_called()

    def test_native_profile_wires_sources_manifest_and_custody(self):
        # This fixture inspects build wiring only. Real controller behavior and
        # real target ELF/import validation are separate integration checks.
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for folder in ('Apps', 'lib', 'scripts'):
                shutil.copytree(ROOT/folder, root/folder)
            shutil.copyfile(ROOT/'LICENSE', root/'LICENSE')
            for name in builder.NATIVE_TIME_CONTROLLER:
                if not (root/name).exists(): (root/name).write_text('/* Wiring fixture only. */\n')
            sdk = {name: ('canonical '+name).encode() for name in (*builder.NATIVE_TIME_SDK_HEADERS, 'LICENSE')}
            elf = b'\x7fELF\x01\x01\x01'+b'\0'*9+b'\x03\x00\x5e\x00'
            target_commands = []

            def run(command, **kwargs):
                if command[-1].endswith('/settings.elf') and '-shared' in command:
                    target_commands.append(command)
                    Path(command[-1]).write_bytes(elf)

            def output(command, **kwargs):
                if command[0].endswith('nm'):
                    return '0000 T app_main\n0001 T app_module_init\n0002 T app_module_fini\n U risc_runtime_get_api\n'
                if command[-1] == '--version': return 'compiler fixture only\n'
                if command == ['git', 'rev-parse', 'HEAD']: return 'fixture-commit\n'
                if command == ['git', 'status', '--porcelain']: return ''
                raise AssertionError(command)

            with mock.patch.object(builder, 'ROOT', root), mock.patch.object(builder, 'native_time_sdk', return_value=sdk), \
                    mock.patch.object(builder.subprocess, 'run', side_effect=run), \
                    mock.patch.object(builder.subprocess, 'check_output', side_effect=output), \
                    contextlib.redirect_stdout(io.StringIO()):
                for profile in ('default', 'x4-desk-clock', 'x4-native-time'):
                    parser, args = self.args('--settings-profile', profile, '--output-dir', str(root/profile))
                    builder.build(args, parser)
            for profile, command in zip(('default', 'x4-desk-clock', 'x4-native-time'), target_commands):
                native = profile == 'x4-native-time'
                out = root/profile
                manifest = json.loads((out/'settings.json').read_text())
                record = json.loads((out/'settings-build-record.json').read_text())
                requirements = manifest['requires']
                self.assertEqual({'capability': 'runtime.realtime-control', 'api': 1} in requirements, native)
                self.assertIn({'capability': 'rtc.clock', 'api': 2}, requirements)
                self.assertIn({'capability': 'storage.key-value', 'api': 1}, requirements)
                self.assertNotIn('runtime.provider-promotion', {r['capability'] for r in requirements})
                self.assertEqual({'capability':'board.battery','api':1} in requirements,native)
                self.assertEqual((out/'native-time-sdk').exists(), native)
                self.assertEqual('invocation_retention' in record, native)
                for name in builder.NATIVE_TIME_SOURCES:
                    self.assertEqual(str(root/name) in command, native)
                for flag in ('PORTABLE_SETTINGS_NATIVE_TIME', 'PORTABLE_SETTINGS_TIME_ZONE', 'PORTABLE_NATIVE_CUSTODY_FENCE'):
                    self.assertEqual('-D'+flag in record['build_defines'], native)
                self.assertEqual(record['sha256'], hashlib.sha256(elf).hexdigest())
                self.assertEqual(record['size_bytes'], len(elf))
                if not native:
                    expected = json.loads((ROOT/('Apps/settings.json' if profile == 'default' else
                        'lib/PortableApps/profiles/x4-desk-clock-settings.json')).read_text())['version']
                    self.assertEqual(record['version'], expected)
                    self.assertEqual(record['display_rotation'], 0)
                    self.assertFalse(record['navigation'])
                    continue
                self.assertEqual(manifest['version'], '1.3.7')
                self.assertEqual(record['time_policy'], 'native-realtime-iana')
                self.assertEqual(record['native_time_runtime_commit'], builder.NATIVE_TIME_RUNTIME_COMMIT)
                self.assertEqual(record['native_time_control_instance'], 0)
                self.assertEqual(record['rtc_access'], 'explicit-save-only')
                self.assertEqual(record['grant_count'],len(requirements))
                self.assertEqual(record['grant_count'],7)
                self.assertEqual(record['required_grants'],[
                    dict(requirement,instance_id=1 if requirement['capability']=='storage.key-value' else 0)
                    for requirement in requirements])
                self.assertTrue(record['mode_capabilities']['explicit_checked_set_time'])
                self.assertFalse(record['mode_capabilities']['sleep_backend'])
                self.assertEqual(record['preferences']['instance'], 1)
                self.assertEqual(record['preferences']['time_zone_key'], 'time_zone')
                self.assertEqual(record['preferences']['rtc_basis_key'], 'rtc_basis')
                self.assertEqual(record['display_rotation'], 90)
                self.assertTrue(record['navigation'])
                self.assertEqual(record['sleep_modes'], ['light', 'deep'])
                self.assertEqual(record['desk_clock_faces'], ['Segments', 'Sans', 'Serif', 'Minimal', 'Railway', 'Deco'])
                for name in (*builder.NATIVE_TIME_SOURCES, *builder.NATIVE_TIME_CONTROLLER, 'LICENSE'):
                    self.assertEqual(record['source_sha256'][name], hashlib.sha256((root/name).read_bytes()).hexdigest())
                for name in builder.NATIVE_TIME_SDK_HEADERS:
                    self.assertEqual(record['native_time_sdk_headers'][name], hashlib.sha256(sdk[name]).hexdigest())
                licenses = out/'licenses/portable-settings'
                self.assertEqual((licenses/'Runtime-LICENSE.txt').read_bytes(), sdk['LICENSE'])
                self.assertEqual((licenses/'TIMEZONE-PROVENANCE.json').read_bytes(),
                                 (ROOT/'lib/PortableApps/time/TIMEZONE_PROVENANCE.json').read_bytes())
                provenance = json.loads((licenses/'native-time-SDK-SOURCES.json').read_text())
                self.assertEqual(provenance['commit'], builder.NATIVE_TIME_RUNTIME_COMMIT)
                self.assertEqual(provenance['source_sha256']['LICENSE'], record['native_time_runtime_license_sha256'])


if __name__ == '__main__':
    unittest.main()

"""Diagnostic SDK/receipt selection; compiler mocks do not qualify target behavior."""
import argparse
import contextlib
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'scripts'))
import build_paper_clock as clock
import build_portable_settings as settings
import build_portable_springboard as springboard
import portable_alarm_build as alarm
import portable_native_toolbar_build as native
import portable_performance_build as performance


def diagnostic_source():
    return {
        'sdk/app/RiscRuntimeV1.h': b'#pragma once\n#define SDK_VERSION 2\n#define RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE 32\n',
        'sdk/app/RiscPerformanceV1.h': b'#pragma once\n#include "RiscRuntimeV1.h"\n_Static_assert(SDK_VERSION == 2, "wrong quoted prefix");\n',
        'sdk/app/RiscRealtimeV1.h': b'/* canonical realtime */\n',
        'sdk/app/RiscRetainedWakeV1.h': b'/* canonical retained wake */\n',
        'sdk/app/RiscProviderPromotionV1.h': b'/* canonical promotion */\n',
        'sdk/driver/RiscLightSleepV1.h': b'/* canonical light sleep */\n',
        'sdk/driver/RiscDeepSleepV1.h': b'/* canonical deep sleep */\n',
        'sdk/driver/RiscTimedSleepV1.h': b'/* canonical timed sleep */\n',
        'LICENSE': b'Canonical Runtime license\n',
    }


class PerformanceBuildProfiles(unittest.TestCase):
    def test_read_uses_immutable_objects_for_complete_sdk_and_license(self):
        source = diagnostic_source()
        parser = argparse.ArgumentParser()
        args = argparse.Namespace(performance_runtime_repo=Path('/runtime'))

        def output(command, **kwargs):
            self.assertEqual(command[:3], ['git', '-C', '/runtime'])
            if command[3] == 'ls-tree':
                self.assertEqual(command[4:], ['-r', '--name-only', performance.RUNTIME_COMMIT, '--', 'sdk'])
                return '\n'.join(path for path in source if path != 'LICENSE')
            self.assertEqual(command[3], 'show')
            commit, path = command[4].split(':', 1)
            self.assertEqual(commit, performance.RUNTIME_COMMIT)
            return source[path]

        with mock.patch.object(performance.subprocess, 'check_output', side_effect=output) as read:
            self.assertEqual(performance.read(args, parser), source)
        self.assertEqual(read.call_count, len(source) + 1)
        with mock.patch.object(performance.subprocess, 'check_output', side_effect=OSError('missing object')), \
                contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            performance.read(args, parser)

    def test_staging_replaces_quoted_prefix_and_preserves_source_tree(self):
        source = diagnostic_source()
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory)
            base = out/'base'
            shutil.copytree(ROOT/'lib/PortableApps/include', base)
            (base/'RiscRuntimeV1.h').write_text('#pragma once\n#define SDK_VERSION 1\n')
            (base/'portable_fixture.h').write_text('#include "RiscRuntimeV1.h"\n')
            includes, receipt = performance.stage(ROOT, out, source, base)
            self.assertEqual((base/'RiscRuntimeV1.h').read_text(), '#pragma once\n#define SDK_VERSION 1\n')
            for path, data in source.items():
                self.assertEqual((includes.parent/path).read_bytes(), data)
                self.assertEqual(receipt['source_sha256'][path], hashlib.sha256(data).hexdigest())
                if path.endswith('.h'):
                    self.assertEqual((includes/Path(path).name).read_bytes(), data)
            self.assertEqual((includes.parent/'time/denver/display_time.h').read_bytes(),
                             (ROOT/'lib/PortableApps/time/denver/display_time.h').read_bytes())
            unit = out/'quoted.c'
            unit.write_text('#include "portable_fixture.h"\n#include "RiscPerformanceV1.h"\nint main(void) { return 0; }\n')
            subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
                            '-I'+str(includes), str(unit), '-o', str(out/'quoted')], check=True)
            self.assertEqual((out/'licenses/performance-runtime/Runtime-LICENSE.txt').read_bytes(), source['LICENSE'])
            self.assertEqual(json.loads((out/'licenses/performance-runtime/SOURCES.json').read_text()), receipt)
            (includes/'stale.h').write_text('obsolete')
            includes, _ = performance.stage(ROOT, out, source, base)
            self.assertFalse((includes/'stale.h').exists())

    def test_requires_explicit_native_profile_before_staging_or_compilation(self):
        cases = [('build_paper_clock.py', '--sparse-start'),
                 ('build_portable_springboard.py', '--time-profile x4-native-time'),
                 ('build_portable_settings.py', '--settings-profile x4-native-time')]
        for builder, requirement in cases:
            with self.subTest(builder=builder), tempfile.TemporaryDirectory() as directory:
                out = Path(directory)/'must-not-exist'
                result = subprocess.run([sys.executable, str(ROOT/'scripts'/builder),
                    '--performance-runtime-repo', '/missing-runtime', '--output-dir', str(out)],
                    capture_output=True, text=True, env=dict(os.environ, NATIVE_APP_CC='/must-not-run'))
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertIn('requires '+requirement, result.stderr)
                self.assertFalse(out.exists())
                result = subprocess.run([sys.executable, str(ROOT/'scripts'/builder),
                    '--performance-display-sdk', '/missing-display', '--output-dir', str(out)],
                    capture_output=True, text=True, env=dict(os.environ, NATIVE_APP_CC='/must-not-run'))
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertIn('--performance-display-sdk requires --performance-runtime-repo', result.stderr)
                self.assertFalse(out.exists())

    def test_incomplete_display_sdk_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            sdk = Path(directory)
            (sdk/'RiscDisplayOutputMetricsV1.h').write_bytes(b'metrics alone')
            args = argparse.Namespace(performance_display_sdk=sdk)
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                performance.read_display(args, argparse.ArgumentParser())

    def test_three_builders_select_flags_and_report_diagnostic_sdk(self):
        source = diagnostic_source()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for folder in ('Apps', 'lib', 'scripts'):
                shutil.copytree(ROOT/folder, root/folder)
            shutil.copyfile(ROOT/'LICENSE', root/'LICENSE')
            runtime = root/'input-runtime/sdk/app'
            runtime.mkdir(parents=True)
            driver = runtime.parent/'driver'
            driver.mkdir()
            for path, data in source.items():
                if path.startswith('sdk/'):
                    target = root/'input-runtime'/path
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(data)
            # Deliberately stale worktree inputs must not be labeled dfc0af5.
            (runtime/'RiscRetainedWakeV1.h').write_bytes(b'old retained header')
            for name in ('RiscDisplayOutputV1.h', 'RiscDisplayOutputPowerV1.h',
                         'RiscTouchV1.h', 'RiscTouchPowerV1.h', 'RiscStorageVolumeV1.h'):
                (driver/name).write_bytes(('deployment '+name).encode())
            sleep = root/'sleep.c'
            sleep.write_text('/* local sleep fixture */\n')
            display = root/'display-sdk'
            display.mkdir()
            for name in performance.DISPLAY_HEADERS:
                (display/name).write_bytes(('canonical display '+name).encode())
            elf = b'\x7fELF\x01\x01\x01'+b'\0'*9+b'\x03\x00\x5e\x00'
            commands = []

            def run(command, **kwargs):
                if '-shared' in command:
                    commands.append(command)
                    Path(command[-1]).write_bytes(elf)

            def output(command, **kwargs):
                if str(command[0]).endswith('nm'):
                    return '0000 T app_main\n0001 T app_module_init\n0002 T app_module_fini\n U risc_runtime_get_api\n'
                if command[-1] == '--version': return 'compiler fixture only\n'
                if command == ['git', 'rev-parse', 'HEAD']: return 'fixture-commit\n'
                if command == ['git', 'status', '--porcelain']: return ''
                if command[0] == 'git' and command[3] == 'show':
                    ref, path = command[-1].split(':', 1)
                    self.assertEqual(ref, alarm.UTILITIES_COMMIT)
                    return ('tagged '+path).encode()
                raise AssertionError(command)

            cases = [
                (clock, 'build-evidence.json', ['--desk-clock', '--sparse-start', '--navigation', '--alarm-client',
                    '--sleep-capability', 'x4.power', '--sleep-sdk', str(driver), '--retained-wake-sdk', str(runtime),
                    '--local-sleep-source', str(sleep)]),
                (springboard, 'springboard-build-record.json', ['--time-profile', 'x4-native-time',
                    '--alarm-client', '--tagged-alarm-utilities', '/utilities']),
                (settings, 'settings-build-record.json', ['--settings-profile', 'x4-native-time']),
            ]
            for module, filename, flags in cases*2:
                display_on = len(commands) >= 3
                with self.subTest(builder=module.__name__, display_metrics=display_on):
                    out = root/(module.__name__+('-metrics' if display_on else ''))
                    args = [*flags, '--performance-runtime-repo', '/diagnostic-runtime', '--output-dir', str(out)]
                    if display_on:
                        args += ['--performance-display-sdk', str(display)]
                    with mock.patch.object(module, 'ROOT', root), \
                            mock.patch.object(performance, 'read', return_value=source), \
                            mock.patch.object(subprocess, 'run', side_effect=run), \
                            mock.patch.object(subprocess, 'check_output', side_effect=output), \
                            mock.patch.object(sys, 'argv', [module.__name__, *args]), \
                            contextlib.redirect_stdout(io.StringIO()):
                        if module is settings:
                            parser = settings.argument_parser()
                            module.build(parser.parse_args(args), parser)
                        else:
                            module.build()
                    command = commands[-1]
                    record = json.loads((out/filename).read_text())
                    manifest = json.loads((out/('default.json' if module is clock else
                        'springboard.json' if module is springboard else 'settings.json')).read_text())
                    self.assertEqual(manifest['version'], performance.VERSIONS[manifest['id']])
                    self.assertEqual(record['version'], manifest['version'])
                    trace = record['performance_trace']
                    self.assertEqual(trace['commit'], performance.RUNTIME_COMMIT)
                    for path, digest in trace['app_source_sha256'].items():
                        self.assertEqual(digest, hashlib.sha256((root/path).read_bytes()).hexdigest())
                    self.assertIn(performance.DEFINE, command)
                    self.assertIn(performance.DEFINE, record['build_defines'])
                    self.assertEqual(performance.DISPLAY_DEFINE in command, display_on)
                    self.assertEqual(performance.DISPLAY_DEFINE in record['build_defines'], display_on)
                    self.assertEqual('display_metrics' in trace, display_on)
                    if display_on:
                        self.assertEqual(trace['display_metrics']['source_directory'], str(display.resolve()))
                        for name in performance.DISPLAY_HEADERS:
                            content = (display/name).read_bytes()
                            self.assertEqual((out/'performance-sdk/include'/name).read_bytes(), content)
                            self.assertEqual(trace['display_metrics']['sha256'][name], hashlib.sha256(content).hexdigest())
                            self.assertEqual(trace['sdk_headers'][name], hashlib.sha256(content).hexdigest())
                    self.assertIn('-I'+str(out/'performance-sdk/include'), command)
                    self.assertNotIn('-I'+str(root/'lib/PortableApps/include'), command)
                    for path, data in source.items():
                        self.assertEqual(trace['source_sha256'][path], hashlib.sha256(data).hexdigest())
                    if module is clock:
                        self.assertEqual(record['retained_wake_sdk_sha256'], trace['sdk_headers']['RiscRetainedWakeV1.h'])
                        self.assertEqual(record['desk_sdk_headers']['RiscRuntimeV1.h'], trace['sdk_headers']['RiscRuntimeV1.h'])
                        storage = driver/'RiscStorageVolumeV1.h'
                        self.assertEqual(record['desk_sdk_headers'][storage.name], hashlib.sha256(storage.read_bytes()).hexdigest())
                        self.assertEqual(trace['sdk_header_overrides'][storage.name]['source_path'], str(storage.resolve()))
                        self.assertEqual((out/'performance-sdk/include'/storage.name).read_bytes(), storage.read_bytes())
                    if module is springboard:
                        admission = json.loads((out/'x4-native-app.json').read_text())
                        self.assertEqual(admission['runtime_source_revision'], performance.RUNTIME_COMMIT)
                        self.assertEqual(admission['version'], manifest['version'])
                        self.assertEqual(record['native_time_sdk']['commit'], performance.RUNTIME_COMMIT)
                        self.assertEqual(admission['sdk_sha256']['RiscPerformanceV1.h'], trace['sdk_headers']['RiscPerformanceV1.h'])
                        if display_on:
                            self.assertEqual(admission['sdk_sha256']['RiscDisplayOutputMetricsV1.h'],
                                             trace['display_metrics']['sha256']['RiscDisplayOutputMetricsV1.h'])
                        provenance = json.loads((out/'licenses/native-time/SOURCES.json').read_text())
                        self.assertEqual(provenance['commit'], performance.RUNTIME_COMMIT)
                    if module is settings:
                        self.assertEqual(record['native_time_runtime_commit'], performance.RUNTIME_COMMIT)
                        self.assertEqual(record['native_time_sdk_headers']['RiscPerformanceV1.h'], trace['sdk_headers']['RiscPerformanceV1.h'])
                        provenance = json.loads((out/'licenses/portable-settings/native-time-SDK-SOURCES.json').read_text())
                        self.assertEqual(provenance['commit'], performance.RUNTIME_COMMIT)

    def test_normal_native_toolbar_preserved_sdk_pin_and_compiled_headers(self):
        self.assertEqual(native.RUNTIME_COMMIT, '274bc66f193cbe29018d2a85c9400cb0dce8aacc')
        self.assertEqual(settings.NATIVE_TIME_RUNTIME_COMMIT, '274bc66f193cbe29018d2a85c9400cb0dce8aacc')
        parser = argparse.ArgumentParser()
        args = argparse.Namespace(time_profile='x4-native-time', native_time_runtime_repo=Path('/runtime'),
                                  tagged_alarm_utilities=None)
        with tempfile.TemporaryDirectory() as directory, \
                mock.patch.object(subprocess, 'check_output', return_value=b'normal canonical') as read:
            includes, flags, _, receipt = native.configure(args, parser, ROOT, Path(directory), 'springboard')
            self.assertNotIn(performance.DEFINE, flags)
            self.assertNotIn('performance_trace', receipt)
            self.assertEqual(receipt['native_time_sdk']['commit'], native.RUNTIME_COMMIT)
            self.assertFalse((includes/'RiscPerformanceV1.h').exists())
            for name in native.SDK_HEADERS:
                self.assertEqual((includes/name).read_bytes(), b'normal canonical')
            self.assertEqual([call.args[0] for call in read.call_args_list], [
                ['git', '-C', '/runtime', 'show', native.RUNTIME_COMMIT+':'+path]
                for path in ('sdk/app/RiscRuntimeV1.h', 'sdk/app/RiscRealtimeV1.h', 'LICENSE')])

    def test_shared_native_helper_rejects_unselected_diagnostic_apps(self):
        args = argparse.Namespace(time_profile='x4-native-time', performance_runtime_repo=Path('/runtime'))
        for app in ('file_browser', 'wifi'):
            with self.subTest(app=app), mock.patch.object(performance, 'read') as read, \
                    contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                native.configure(args, argparse.ArgumentParser(), ROOT, Path('/must-not-stage'), app)
            read.assert_not_called()


if __name__ == '__main__':
    unittest.main()

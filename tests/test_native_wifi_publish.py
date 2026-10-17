"""Publication contract tests; no external release is mutated."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'scripts'))
import build_native_wifi_qualification as builder
import publish_native_wifi as publisher


def sha(data):
    return hashlib.sha256(data).hexdigest()


class NativeWifiPublication(unittest.TestCase):
    def fixture(self, root):
        app = root/'app'; app.mkdir()
        elf = b'\x7fELF\x01\x01\x01'+b'\0'*9+b'\x03\x00\x5e\x00'+b'1.4.1\0'
        command = builder.compose_arguments((ROOT/builder.RECIPE).read_bytes(),
            {'bridges': Path('/bridges')}, Path('/out'), Path('/include'),
            Path('/profile'), Path('/sdk'), '1.4.1')
        layout = {'sha256': sha(elf), 'file_size_bytes': len(elf),
                  'linked_device_memory_fit_qualified': False,
                  'section_loader': {'data_allocation_request_bytes': 1234}}
        command_bytes = json.dumps(command).encode(); layout_bytes = json.dumps(layout).encode()
        record = {'repository_commit': 'new', 'working_tree_dirty': False,
                  'bundle': publisher.TAG, 'version': '1.4.1', 'size_bytes': len(elf), 'sha256': sha(elf),
                  'required_capabilities': builder.manifest('1.4.1')['requires'],
                  'software_target_validation': True, 'compiled_version_verified': True, 'native_ui': True,
                  'native_quick': True, 'native_alarm': True,
                  'atomic_wifi_persistence': True, 'settings_availability': True, 'wifi_profiles': True,
                  'shared_text_input': True, 'async_rf_cleanup': True, 'host_fixture_excluded': True,
                  'hardware_verified': False, 'firmware_or_product_replacement': False,
                  'external_sources': {name: {'repository': builder.REPOSITORIES[name], 'commit': pin}
                                       for name, pin in builder.PINS.items()},
                  'command_sha256': sha(command_bytes), 'elf_layout_sha256': sha(layout_bytes),
                  'build_defines': [item for item in command if item.startswith('-D')],
                  'exports': sorted(builder.EXPORTS), 'imports': ['risc_runtime_get_api'],
                  'notice_sha256': {'licenses/LICENSE.txt': sha(b'license')}}
        (app/'licenses').mkdir(); (app/'licenses/LICENSE.txt').write_bytes(b'license')
        (app/'wifi_settings.elf').write_bytes(elf)
        (app/'wifi-build-record.json').write_text(json.dumps(record))
        (app/'wifi_settings.json').write_text(json.dumps(builder.manifest('1.4.1')))
        (app/'command.json').write_bytes(command_bytes); (app/'elf-layout.json').write_bytes(layout_bytes)
        admission = {'application': 'wifi_settings', 'version': '1.4.1',
                     'runtime_commit': builder.PINS['runtime'], 'elf_sha256': sha(elf),
                     'elf_layout_sha256': sha(layout_bytes),
                     'manifest_sha256': sha((app/'wifi_settings.json').read_bytes()),
                     'required_capabilities': record['required_capabilities'],
                     'images': {'native': layout}, 'linked_device_memory_fit_qualified': False,
                     'admission_runs': [{'image': 'native-wifi', 'sanitized': sanitized,
                         'result': {'ordinary_application_admitted': True,
                                    'missing_import_rejected': True, 'unsafe_state_rejected': True,
                                    'driver_role_rejected': True, 'alignment_placements_checked': 64,
                                    'data_allocation_request_bytes': 1234,
                                    'linked_device_memory_fit_qualified': False}}
                         for sanitized in (False, True)]}
        (app/'elf-memory-admission.json').write_text(json.dumps(admission))
        return app

    def invoke(self, root, fake):
        with patch.object(sys, 'argv', ['publish', '--artifact', str(root),
                                       '--repository', 'owner/repo', '--source-sha', 'new']), \
             patch.object(publisher, 'gh', fake):
            publisher.main()

    def test_new_release_uses_qualified_source_and_draft_before_upload(self):
        calls = []
        def fake(*args, **kwargs):
            calls.append(args)
            if args[:2] == ('release', 'view') or args[0] == 'api':
                return subprocess.CompletedProcess([], 1, '', 'HTTP 404: Not Found')
            if args[:2] == ('release', 'upload'):
                paths = [Path(x) for x in args[5:]]
                self.assertEqual({x.name for x in paths}, publisher.REQUIRED)
                self.assertTrue(all(x.is_file() for x in paths))
                sums = (paths[0].parent/'SHA256SUMS').read_text()
                self.assertIn(sha((paths[0].parent/'wifi_settings.elf').read_bytes()), sums)
            return subprocess.CompletedProcess([], 0, '', '')
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary); self.fixture(root)
            with patch.object(publisher, 'validate_native_overlay_evidence', return_value={'checked': True}) as gate:
                self.invoke(root, fake)
                gate.assert_called_once()
        mutations = [c for c in calls if c[:2] in [('release','create'), ('release','upload'), ('release','edit')]]
        self.assertEqual([c[1] for c in mutations], ['create', 'upload', 'edit'])
        self.assertIn('--draft', mutations[0])
        self.assertEqual(mutations[0][mutations[0].index('--target')+1], 'new')
        self.assertIn('--draft=false', mutations[-1])

    def test_existing_release_verifies_original_source_and_skips_new_source(self):
        calls = []
        def fake(*args, **kwargs):
            calls.append(args)
            if args[:2] == ('release', 'view'):
                return subprocess.CompletedProcess([], 0, json.dumps({'isDraft': False, 'isPrerelease': True,
                    'assets': [{'name': n} for n in publisher.REQUIRED]}), '')
            if args[:2] == ('release', 'download'):
                (Path(args[args.index('--dir')+1])/'publication.json').write_text(
                    json.dumps({'tag': publisher.TAG, 'source_commit': 'old'}))
            return subprocess.CompletedProcess([], 0, 'old\n', '')
        self.invoke(Path('/does-not-exist'), fake)
        self.assertFalse(any(c[:2] in [('release','create'), ('release','upload'), ('release','edit')] for c in calls))

    def test_incomplete_prior_release_is_never_modified(self):
        for fields in ({'isDraft': True, 'isPrerelease': True}, {'isDraft': False, 'isPrerelease': False},
                       {'isDraft': False, 'isPrerelease': True}):
            calls = []
            def fake(*args, **kwargs):
                calls.append(args)
                return subprocess.CompletedProcess([], 0, json.dumps(dict(fields, assets=[])), '')
            with self.assertRaises(RuntimeError):
                self.invoke(Path('/does-not-exist'), fake)
            self.assertEqual(len(calls), 1)

    def test_artifact_tampering_is_rejected_before_release_mutation(self):
        cases = ('wrong-source', 'dirty', 'same-count-capability-substitution', 'missing-native-feature',
                 'altered-command', 'altered-elf', 'altered-layout', 'altered-license', 'wrong-pin',
                 'missing-atomic-proof', 'wrong-exports', 'no-notices', 'unsafe-notice-path',
                 'missing-admission', 'wrong-admission-elf', 'wrong-admission-layout',
                 'wrong-admission-manifest', 'wrong-admission-version', 'wrong-admission-capabilities',
                 'wrong-admission-pin', 'missing-sanitized-admission', 'failed-admission',
                 'wrong-allocation', 'missing-placement', 'hardware-claim')
        for case in cases:
            calls = []
            def fake(*args, **kwargs):
                calls.append(args)
                return subprocess.CompletedProcess([], 1, '', 'HTTP 404: Not Found')
            with self.subTest(case=case), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary); app = self.fixture(root)
                record = json.loads((app/'wifi-build-record.json').read_text())
                if case == 'wrong-source': record['repository_commit'] = 'wrong'
                elif case == 'dirty': record['working_tree_dirty'] = True
                elif case == 'same-count-capability-substitution':
                    manifest = builder.manifest('1.4.1'); manifest['requires'][0]['capability'] = 'fake'
                    record['required_capabilities'] = manifest['requires']
                    (app/'wifi_settings.json').write_text(json.dumps(manifest))
                elif case == 'missing-native-feature':
                    command = json.loads((app/'command.json').read_text()); command.remove('-DPORTABLE_WIFI_NATIVE_UI')
                    data = json.dumps(command).encode(); (app/'command.json').write_bytes(data)
                    record['command_sha256'] = sha(data)
                    record['build_defines'] = [item for item in command if item.startswith('-D')]
                elif case == 'altered-command': (app/'command.json').write_text('[]')
                elif case == 'altered-elf': (app/'wifi_settings.elf').write_bytes(b'bad')
                elif case == 'altered-layout': (app/'elf-layout.json').write_text('{}')
                elif case == 'altered-license': (app/'licenses/LICENSE.txt').write_bytes(b'bad')
                elif case == 'wrong-pin': record['external_sources']['runtime']['commit'] = 'bad'
                elif case == 'missing-atomic-proof': record['atomic_wifi_persistence'] = False
                elif case == 'wrong-exports': record['exports'] = ['unexpected']
                elif case == 'no-notices': record['notice_sha256'] = {}
                elif case == 'unsafe-notice-path': record['notice_sha256'] = {'../outside': sha(b'license')}
                elif case == 'missing-admission': (app/'elf-memory-admission.json').unlink()
                elif case in ('wrong-admission-elf', 'wrong-admission-layout', 'wrong-admission-manifest',
                              'wrong-admission-version', 'wrong-admission-capabilities', 'wrong-admission-pin',
                              'missing-sanitized-admission', 'failed-admission', 'wrong-allocation',
                              'missing-placement', 'hardware-claim'):
                    path = app/'elf-memory-admission.json'; admission = json.loads(path.read_text())
                    if case == 'wrong-admission-elf': admission['elf_sha256'] = 'wrong'
                    elif case == 'wrong-admission-layout': admission['elf_layout_sha256'] = 'wrong'
                    elif case == 'wrong-admission-manifest': admission['manifest_sha256'] = 'wrong'
                    elif case == 'wrong-admission-version': admission['version'] = 'old'
                    elif case == 'wrong-admission-capabilities': admission['required_capabilities'] = []
                    elif case == 'wrong-admission-pin': admission['runtime_commit'] = 'wrong'
                    elif case == 'missing-sanitized-admission': admission['admission_runs'].pop()
                    elif case == 'failed-admission': admission['admission_runs'][1]['result']['ordinary_application_admitted'] = False
                    elif case == 'wrong-allocation': admission['admission_runs'][0]['result']['data_allocation_request_bytes'] = 1
                    elif case == 'missing-placement': admission['admission_runs'][0]['result']['alignment_placements_checked'] = 0
                    elif case == 'hardware-claim': admission['linked_device_memory_fit_qualified'] = True
                    path.write_text(json.dumps(admission))
                (app/'wifi-build-record.json').write_text(json.dumps(record))
                with self.assertRaises((ValueError, RuntimeError)):
                    self.invoke(root, fake)
                self.assertFalse(any(c[:2] in [('release','create'), ('release','upload'), ('release','edit')] for c in calls))

    def test_prior_tag_for_different_source_blocks_new_release(self):
        calls = []
        def fake(*args, **kwargs):
            calls.append(args)
            if args[0] == 'api': return subprocess.CompletedProcess([], 0, 'old\n', '')
            return subprocess.CompletedProcess([], 1, '', 'HTTP 404: Not Found')
        with self.assertRaisesRegex(RuntimeError, 'another source'):
            self.invoke(Path('/does-not-exist'), fake)
        self.assertFalse(any(c[:2] == ('release','create') for c in calls))


if __name__ == '__main__':
    unittest.main()

"""Offline publication gate tests. All release calls are fakes; no C compiler runs."""
import copy
from contextlib import ExitStack, redirect_stdout
import hashlib
import io
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'scripts'))
import native_overlay_evidence as evidence
import publish_native_settings
import publish_native_wifi
import build_native_wifi_qualification as wifi_builder


def raw_sha(data):
    return hashlib.sha256(data).hexdigest()


def write_json(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2)+'\n')


class EvidenceFixture:
    """Coherent synthetic bytes, with independent immutable baseline anchors."""
    source = 'a'*40

    def __init__(self, parent, app):
        self.root = parent/'checkout'; self.artifact = parent/'artifact'; self.app = app
        self.publisher = publish_native_settings if app == 'settings' else publish_native_wifi
        self.appdir = self.artifact/'app'; self.appdir.mkdir(parents=True)
        version = '1.4.2' if app == 'settings' else '1.4.1'
        old = json.loads((ROOT/'docs/native-overlays'/('released-'+app)/(app+'-build-record.json')).read_text())
        self.manifest = (dict(json.loads((ROOT/'docs/native-overlays/released-settings/settings.json').read_text()),
                              version=version) if app == 'settings' else wifi_builder.manifest(version))
        if app == 'wifi':
            command = wifi_builder.compose_arguments((ROOT/wifi_builder.RECIPE).read_bytes(),
                {'bridges': Path('/bridges')}, Path('/out'), Path('/include'), Path('/profile'), Path('/sdk'), version)
        else:
            command = ['cc', *[v.replace('1.4.1', version) for v in old['build_defines']],
                       '-DPORTABLE_QUICK_NATIVE', '-DPORTABLE_ALARM_NATIVE']
        sources = {k[7:] for k in old['source_sha256'] if k.startswith('system:')}
        sources.update('lib/PortableApps/src/'+n for n in ('alarm_native.inc',
                       'raster_snapshot_state.inc', 'raster_snapshot_replay.inc', *evidence.QUICK_SOURCES))
        sources.update('test/native_apps/'+n for n in evidence.FIXTURES)
        sources.update('test/native_apps/native_overlay_'+n+'_test.c' for n in ('settings', 'wifi', 'providers'))
        sources.update('scripts/'+n for n in ('test_native_overlay_apps.py', 'test_native_settings.py'))
        sources.update('lib/PortableApps/include/'+n for n in ('RiscRuntimeV1.h', 'RiscKeyValueAtomicV1.h'))
        for relative in sources:
            p = self.root/relative; p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text('synthetic source: '+relative+'\n')
        self.runtime = {Path(k).name: ('synthetic pinned Runtime: '+k+'\n').encode()
                        for k in old['source_sha256'] if k.startswith('runtime:sdk/app/')}
        self.source_hashes = {'system:'+r: evidence.sha(self.root/r) for r in sources
                             if not r.startswith(('test/', 'scripts/test_'))}
        self.source_hashes.update({'runtime:sdk/app/'+k: raw_sha(v) for k, v in self.runtime.items()})
        # Synthetic immutable baselines use the same identities and pinned recipe,
        # but their independently anchored byte hashes are injected only in tests.
        old['source_sha256'] = copy.deepcopy(self.source_hashes)
        self.record = copy.deepcopy(old)
        elf = b'\x7fELF\x01\x01\x01'+b'\0'*9+b'\x03\x00\x5e\x00'+version.encode()+b'\0'
        self.record.update(repository_commit=self.source, working_tree_dirty=False,
            bundle=self.publisher.TAG, version=version, size_bytes=len(elf), sha256=raw_sha(elf),
            required_capabilities=self.manifest['requires'], software_target_validation=True,
            compiled_version_verified=True, native_ui=True, native_quick=True, native_alarm=True,
            atomic_wifi_persistence=True, settings_availability=True, wifi_profiles=True,
            shared_text_input=True, async_rf_cleanup=True, host_fixture_excluded=True,
            hardware_verified=False, firmware_or_product_replacement=False,
            build_defines=[v for v in command if v.startswith('-D')],
            notice_sha256={'licenses/LICENSE.txt': raw_sha(b'license')})
        elf_name = 'settings.elf' if app == 'settings' else 'wifi_settings.elf'
        manifest_name = 'settings.json' if app == 'settings' else 'wifi_settings.json'
        self.memory_name = 'memory-admission.json' if app == 'settings' else 'elf-memory-admission.json'
        (self.appdir/elf_name).write_bytes(elf)
        write_json(self.appdir/manifest_name, self.manifest)
        write_json(self.appdir/'command.json', command)
        self.record['command_sha256'] = evidence.sha(self.appdir/'command.json')
        (self.appdir/'licenses').mkdir(); (self.appdir/'licenses/LICENSE.txt').write_bytes(b'license')
        loader = {'within_structural_limit': True, 'executable_allocation_request_bytes': 100,
                  'data_allocation_request_bytes': 200, 'resident_section_allocation_request_bytes': 300,
                  'uncached_input_plus_section_allocations_bytes': 400, 'structural_limit_bytes': 1000000}
        layout = {'sha256': self.record['sha256'], 'file_size_bytes': len(elf),
                  'linked_device_memory_fit_qualified': False, 'section_loader': loader}
        write_json(self.appdir/'elf-layout.json', layout)
        self.record['elf_layout_sha256'] = evidence.sha(self.appdir/'elf-layout.json')
        memory = {'images': {'native': layout}, 'application': self.manifest['id'], 'version': version,
                  'runtime_commit': evidence.RUNTIME_PIN, 'elf_sha256': self.record['sha256'],
                  'elf_layout_sha256': self.record['elf_layout_sha256'],
                  'manifest_sha256': evidence.sha(self.appdir/manifest_name),
                  'required_capabilities': self.manifest['requires'],
                  'linked_device_memory_fit_qualified': False, 'admission_scope': 'software-only',
                  'unqualified_requirements': ['physical device fit'],
                  'admission_runs': [{'image': 'native-wifi', 'sanitized': san, 'result': {
                      'ordinary_application_admitted': True, 'missing_import_rejected': True,
                      'unsafe_state_rejected': True, 'driver_role_rejected': True,
                      'alignment_placements_checked': 64, 'data_allocation_request_bytes': 200,
                      'linked_device_memory_fit_qualified': False}} for san in (False, True)]}
        write_json(self.appdir/self.memory_name, memory)
        write_json(self.appdir/(app+'-build-record.json'), self.record)
        baseline = self.root/'docs/native-overlays'/('released-'+app)
        write_json(baseline/(app+'-build-record.json'), old)
        baseline_memory = copy.deepcopy(memory)
        baseline_memory['images']['native'].update(sha256=old['sha256'], file_size_bytes=old['size_bytes'])
        write_json(baseline/self.memory_name, baseline_memory)
        write_json(baseline/'command.json', ['cc', *old['build_defines']])
        write_json(baseline/manifest_name, dict(self.manifest, version=old['version']))
        tag, source, _ = evidence.BASELINES[app]
        assets = {p.name: evidence.sha(p) for p in baseline.iterdir()}
        controls = {app: {'tag': tag, 'source_commit': source, 'assets': assets,
                         'release_url': 'https://github.com/example/releases/tag/'+tag}}
        write_json(self.root/'docs/native-overlays/released-controls.json', controls)
        self.baselines = {**evidence.BASELINES, app: (tag, source, assets)}
        with patch.object(evidence.comparison, 'ROOT', self.root):
            report = evidence.comparison.compare(app, self.appdir, self.appdir/self.memory_name)
        write_json(self.appdir/'native-overlay-comparison.json', report)
        for sanitized in (False, True):
            self.make_receipt(sanitized)

    def make_receipt(self, sanitized):
        app = self.app
        out = self.artifact/('overlays-sanitized' if sanitized else 'overlays-normal')
        origin = Path('/historic/host-'+app+str(int(sanitized))); checkout = Path('/historic/checkout')
        out.mkdir()
        for folder in ('runtime-sdk', 'include', 'fixtures'):
            (out/folder).mkdir()
        for name, data in self.runtime.items():
            (out/'runtime-sdk'/name).write_bytes(data)
        for p in (self.root/'lib/PortableApps/include').glob('*.h'):
            shutil.copyfile(p, out/'include'/p.name)
        for name in ('RiscRuntimeV1.h', 'RiscKeyValueAtomicV1.h'):
            (out/'include'/name).write_bytes(self.runtime[name])
        for name in evidence.FIXTURES:
            (out/'fixtures'/name).write_text('synthetic staged fixture: '+name+'\n')
        time = self.root/'lib/PortableApps/time'
        if time.exists():
            shutil.copytree(time, out/'time')
        receipt = {'status': 'PASS', 'hardware_tested': False, 'cases': evidence.CASES,
                   'runtime_pin': evidence.RUNTIME_PIN, 'source_sha256': evidence.source_paths(self.root),
                   'builds': [], 'runs': []}
        for key, folder in (('runtime_sdk_sha256', 'runtime-sdk'), ('staged_include_sha256', 'include'),
                            ('fixture_sha256', 'fixtures')):
            receipt[key] = {p.name: evidence.sha(p) for p in (out/folder).iterdir()}
        for w, h in evidence.SIZES:
            for snapshot in (0, 1):
                name = f'{app}-{w}x{h}-snapshot{snapshot}-san{int(sanitized)}'
                defines = [*evidence.COMMON, *evidence.APP_DEFINES[app]]
                macros = {v.partition('=')[0]: v.partition('=')[2] or '1' for v in defines}
                for key in ('PORTABLE_QUICK_ACTIONS', 'PORTABLE_ALARM_CLIENT', *evidence.CONTROLLERS[app]):
                    macros.setdefault(key, '')
                flags = ['-D'+v for v in defines]
                if snapshot:
                    macros['PORTABLE_RASTER_SNAPSHOT'] = '1'; flags += ['-DPORTABLE_RASTER_SNAPSHOT']
                fixture = 'quick_adapter_test.c' if app == 'settings' else 'wifi_availability_test.c'
                flags += ['-DNATIVE_OVERLAY_'+app.upper()+'_FIXTURE='+json.dumps(str(origin/'fixtures'/fixture))]
                if app == 'wifi':
                    flags += [f'-DTEST_WIFI_RGB_WIDTH={w}', f'-DTEST_WIFI_RGB_HEIGHT={h}']
                    macros.update(TEST_WIFI_RGB_WIDTH=str(w), TEST_WIFI_RGB_HEIGHT=str(h))
                if sanitized:
                    flags += list(evidence.SANITIZERS)
                sources = ([str(checkout/'Apps/settings.c')] if app == 'settings' else [])
                test_source = str(checkout/f'test/native_apps/native_overlay_{app}_test.c')
                sources += [test_source, *[str(checkout/'lib/PortableApps/src'/s) for s in evidence.QUICK_SOURCES]]
                if app == 'wifi':
                    sources += [str(checkout/'lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c')]
                prefix = ['cc', '-std=c11', '-O1', '-g0', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function',
                          *flags, '-I'+str(origin/'include'), '-I'+str(origin/'runtime-sdk'),
                          '-I'+str(checkout/'lib/NativeApps/include'), '-I'+str(checkout/'lib/RemoteFiles/vendor/tinydtls_sha2')]
                (out/(name+'-compile.log')).write_text('compiled\n')
                (out/(name+'-macros.txt')).write_text(''.join('#define '+k+(' '+v if v else '')+'\n' for k,v in macros.items()))
                deps = {str(checkout/k[7:]): v for k,v in self.source_hashes.items() if k.startswith('system:')}
                deps.update({test_source: evidence.sha(self.root/f'test/native_apps/native_overlay_{app}_test.c'),
                    str(checkout/'test/native_apps/native_overlay_providers_test.c'):
                        evidence.sha(self.root/'test/native_apps/native_overlay_providers_test.c')})
                for folder, key in (('include', 'staged_include_sha256'), ('fixtures', 'fixture_sha256')):
                    deps.update({str(origin/folder/k): v for k,v in receipt[key].items()})
                build = {'name': name, 'command': [*prefix, *sources, '-o', str(origin/name)],
                         'selected_defines': defines, 'snapshot': snapshot, 'sanitized': sanitized,
                         'returncode': 0, 'binary_sha256': 'b'*64,
                         'log_sha256': evidence.sha(out/(name+'-compile.log')),
                         'macro_sha256': evidence.sha(out/(name+'-macros.txt')),
                         'actual_macros': {k:v for k,v in macros.items() if k.startswith(('PORTABLE_', 'WATCH_'))},
                         'preprocessor_command': [*prefix, '-dM', '-E', test_source],
                         'compiler_dependency_sha256': deps}
                receipt['builds'].append(build)
                for case in range(5):
                    title = 'Settings' if app == 'settings' else 'Wi-Fi'
                    path = out/(name+f'-case{case}.log')
                    path.write_text(f'{title} native overlays {w}x{h} case{case} PASS: synthetic unit-test witness\n')
                    receipt['runs'].append({'build': name, 'case': case, 'returncode': 0,
                        'command': [str(origin/name), *([str(w),str(h)] if app == 'settings' else []), str(case)],
                        'log_sha256': evidence.sha(path)})
        write_json(out/'receipt.json', receipt)

    def context(self):
        stack = ExitStack()
        stack.enter_context(patch.object(evidence, 'ROOT', self.root))
        stack.enter_context(patch.object(evidence.comparison, 'ROOT', self.root))
        stack.enter_context(patch.object(evidence, 'BASELINES', self.baselines))
        return stack

    def validate(self):
        with self.context():
            return evidence.validate_native_overlay_evidence(self.app, self.artifact, self.record, self.source)

    def publish(self, calls):
        def fake(*args, **kwargs):
            calls.append(args)
            if args[:2] == ('release', 'view') or args[0] == 'api':
                return subprocess.CompletedProcess([], 1, '', 'HTTP 404: Not Found')
            if args[:2] == ('release', 'upload'):
                publication = json.loads((Path(args[5]).parent/'publication.json').read_text())
                assert set(publication['native_overlay_evidence']) == {
                    'comparison_sha256', 'normal_receipt_sha256', 'sanitized_receipt_sha256'}
            return subprocess.CompletedProcess([], 0, '', '')
        with self.context(), patch.object(self.publisher, 'gh', fake), patch.object(sys, 'argv', [
                'publish', '--repository', 'owner/repo', '--artifact', str(self.artifact), '--source-sha', self.source]):
            self.publisher.main()


class NativeOverlayEvidence(unittest.TestCase):
    def test_real_baseline_anchors_match_retained_bytes(self):
        controls = evidence.read_json(ROOT/'docs/native-overlays/released-controls.json')
        for app, (tag, source, assets) in evidence.BASELINES.items():
            self.assertEqual((controls[app]['tag'], controls[app]['source_commit'], controls[app]['assets']),
                             (tag, source, assets))
            for name, value in assets.items():
                self.assertEqual(evidence.sha(ROOT/'docs/native-overlays'/('released-'+app)/name), value)

    def test_complete_relocated_evidence_allows_both_publishers(self):
        for app in ('settings', 'wifi'):
            with self.subTest(app=app), tempfile.TemporaryDirectory() as temp:
                fixture = EvidenceFixture(Path(temp), app)
                expected = fixture.validate()
                self.assertEqual(len(expected), 3)
                output = io.StringIO()
                with fixture.context(), redirect_stdout(output), patch.object(sys, 'argv', [
                        'evidence', '--app', app, '--artifact', str(fixture.artifact),
                        '--source-sha', fixture.source]):
                    evidence.main()
                self.assertEqual(json.loads(output.getvalue()), expected)
                calls = []; fixture.publish(calls)
                self.assertEqual([c[1] for c in calls if c[0] == 'release' and c[1] != 'view'],
                                 ['create', 'upload', 'edit'])

    def test_invalid_actual_app_receipts_never_mutate_either_publisher(self):
        cases = ('missing-normal', 'missing-sanitized', 'running', 'failed', 'hardware', 'runtime-pin',
                 'missing-build', 'duplicate-build', 'extra-build', 'wrong-app', 'wrong-mode', 'wrong-snapshot',
                 'missing-run', 'duplicate-run', 'extra-run', 'wrong-case', 'failed-build', 'failed-run',
                 'wrong-run-command', 'missing-selector', 'missing-controller', 'missing-sanitizer',
                 'disabled-assertions', 'macro-record-tamper', 'macro-file-tamper', 'compile-log-tamper',
                 'case-log-tamper', 'missing-pass-marker', 'missing-source', 'stale-source',
                 'stale-dependency', 'missing-dependency', 'unsafe-dependency', 'staged-header-tamper',
                 'staged-fixture-tamper', 'runtime-sdk-tamper', 'missing-source-file', 'duplicate-json-key')
        for app in ('settings', 'wifi'):
            for case in cases:
                with self.subTest(app=app, case=case), tempfile.TemporaryDirectory() as temp:
                    f = EvidenceFixture(Path(temp), app)
                    path = f.artifact/'overlays-sanitized/receipt.json'
                    data = evidence.read_json(path); b = data['builds'][0]; r = data['runs'][0]
                    name = b['name']; out = path.parent
                    if case == 'missing-normal': (f.artifact/'overlays-normal/receipt.json').unlink()
                    elif case == 'missing-sanitized': path.unlink()
                    elif case == 'running': data['status'] = 'RUNNING'
                    elif case == 'failed': data['status'] = 'FAIL'
                    elif case == 'hardware': data['hardware_tested'] = True
                    elif case == 'runtime-pin': data['runtime_pin'] = 'old'
                    elif case == 'missing-build': data['builds'].pop()
                    elif case == 'duplicate-build': data['builds'][-1] = copy.deepcopy(b)
                    elif case == 'extra-build': data['builds'].append(copy.deepcopy(b))
                    elif case == 'wrong-app': b['name'] = b['name'].replace(app, 'another-app')
                    elif case == 'wrong-mode': b['sanitized'] = False
                    elif case == 'wrong-snapshot': b['snapshot'] = 1
                    elif case == 'missing-run': data['runs'].pop()
                    elif case == 'duplicate-run': data['runs'][-1] = copy.deepcopy(r)
                    elif case == 'extra-run': data['runs'].append(copy.deepcopy(r))
                    elif case == 'wrong-case': r['case'] = False
                    elif case == 'failed-build': b['returncode'] = 1
                    elif case == 'failed-run': r['returncode'] = 1
                    elif case == 'wrong-run-command': r['command'][-1] = '4'
                    elif case == 'missing-selector': b['selected_defines'].remove('PORTABLE_QUICK_NATIVE')
                    elif case == 'missing-controller': b['actual_macros'].pop('PORTABLE_ALARM_CLIENT')
                    elif case == 'missing-sanitizer': b['command'].remove('-fsanitize=address,undefined')
                    elif case == 'disabled-assertions': b['command'].insert(1, '-DNDEBUG')
                    elif case == 'macro-record-tamper': b['actual_macros']['PORTABLE_ALARM_NATIVE'] = '0'
                    elif case == 'macro-file-tamper': (out/(name+'-macros.txt')).write_text('#define NDEBUG 1\n')
                    elif case == 'compile-log-tamper': (out/(name+'-compile.log')).write_text('changed')
                    elif case == 'case-log-tamper': (out/(name+'-case0.log')).write_text('changed')
                    elif case == 'missing-pass-marker':
                        log = out/(name+'-case0.log'); log.write_text('not a passing test\n'); r['log_sha256'] = evidence.sha(log)
                    elif case == 'missing-source': data['source_sha256'].pop(next(iter(data['source_sha256'])))
                    elif case == 'stale-source': data['source_sha256']['Apps/'+('settings.c' if app == 'settings' else 'wifi_settings.c')] = '0'*64
                    elif case == 'stale-dependency': b['compiler_dependency_sha256'][next(iter(b['compiler_dependency_sha256']))] = '0'*64
                    elif case == 'missing-dependency': b['compiler_dependency_sha256'].pop('/historic/checkout/lib/PortableApps/src/alarm_native.inc')
                    elif case == 'unsafe-dependency': b['compiler_dependency_sha256']['/historic/../untrusted.c'] = '0'*64
                    elif case == 'staged-header-tamper': (out/'include/RiscRuntimeV1.h').write_text('changed')
                    elif case == 'staged-fixture-tamper': (out/'fixtures/quick_adapter_test.c').write_text('changed')
                    elif case == 'runtime-sdk-tamper': (out/'runtime-sdk/RiscRuntimeV1.h').write_text('changed')
                    elif case == 'missing-source-file': (f.root/'test/native_apps/native_overlay_providers_test.c').unlink()
                    if case not in ('missing-sanitized', 'duplicate-json-key'):
                        write_json(path, data)
                    if case == 'duplicate-json-key': path.write_text('{"status":"FAIL","status":"PASS"}')
                    calls = []
                    with self.assertRaises(RuntimeError): f.publish(calls)
                    self.assertFalse(any(c[:2] in (('release','create'), ('release','upload'), ('release','edit')) for c in calls))

    def test_comparison_and_target_tampering_never_mutates(self):
        cases = ('missing', 'wrong-location', 'application', 'baseline', 'source', 'version', 'elf',
                 'selectors', 'capabilities', 'imports', 'exports', 'pins', 'allocation', 'source-delta',
                 'hardware', 'baseline-bytes', 'baseline-control', 'target-source', 'target-pin', 'target-elf')
        for app in ('settings', 'wifi'):
            for case in cases:
                with self.subTest(app=app, case=case), tempfile.TemporaryDirectory() as temp:
                    f = EvidenceFixture(Path(temp), app); path = f.appdir/'native-overlay-comparison.json'
                    data = evidence.read_json(path)
                    if case == 'missing': path.unlink()
                    elif case == 'wrong-location': path.rename(f.artifact/'native-overlay-comparison.json')
                    elif case == 'application': data['application'] = 'other'
                    elif case == 'baseline': data['baseline']['source_commit'] = 'old'
                    elif case == 'source': data['successor_source'] = 'old'
                    elif case == 'version': data['successor_version'] = 'old'
                    elif case == 'elf': data['successor_elf_sha256'] = '0'*64
                    elif case == 'selectors': data['added_selectors'].pop()
                    elif case == 'capabilities': data['required_capabilities'].pop()
                    elif case == 'imports': data['imports'].pop()
                    elif case == 'exports': data['exports'].pop()
                    elif case == 'pins': data['external_pins_unchanged'] = False
                    elif case == 'allocation': data['allocation_comparison']['data_allocation_request_bytes']['delta'] = 1
                    elif case == 'source-delta': data['source_closure_delta']['changed'].append('invented')
                    elif case == 'hardware': data['linked_device_memory_fit_qualified'] = True
                    elif case == 'baseline-bytes': (f.root/'docs/native-overlays'/('released-'+app)/'command.json').write_text('[]')
                    elif case == 'baseline-control':
                        control = f.root/'docs/native-overlays/released-controls.json'; c = evidence.read_json(control)
                        c[app]['tag'] = 'moving'; write_json(control, c)
                    elif case == 'target-source': (f.root/'lib/PortableApps/src/quick_adapter.inc').write_text('changed')
                    elif case == 'target-pin':
                        f.record['external_sources']['runtime']['commit'] = 'old'
                        write_json(f.appdir/(app+'-build-record.json'), f.record)
                    elif case == 'target-elf': (f.appdir/('settings.elf' if app == 'settings' else 'wifi_settings.elf')).write_bytes(b'changed')
                    if case not in ('missing', 'wrong-location'): write_json(path, data)
                    calls = []
                    with self.assertRaises(RuntimeError): f.publish(calls)
                    self.assertFalse(any(c[:2] in (('release','create'), ('release','upload'), ('release','edit')) for c in calls))


if __name__ == '__main__':
    unittest.main()

"""Fail-closed, offline publication checks for actual-app native overlay evidence.

Host executions establish controller behavior only. Target comparison establishes
recipe/ABI and structural admission only; neither establishes physical fit.
Absolute paths in CI receipts are treated as provenance, never as read authority.
"""
import hashlib
import json
from pathlib import Path
import re

import report_native_overlay_successor as comparison

ROOT = Path(__file__).resolve().parents[1]
RUNTIME_PIN = '48229bd1593a2e24e95bf4537b495ee615990fd3'
CASES = {'0': 'quick-draft-restoration', '1': 'paired-atomic-preferences-and-reentry',
         '2': 'alarm-interrupts-quick', '3': 'alarm-interrupts-torch',
         '4': 'queued-stale-and-fresh-app-contact'}
SIZES = ((240, 240), (410, 502))
COMMON = ('PORTABLE_NATIVE_CANVAS', 'PORTABLE_NOVA_UI', 'PORTABLE_NATIVE_CUSTODY_FENCE',
          'PORTABLE_CHECKED_STORAGE_RUNTIME', 'PORTABLE_ALARM_TERMINAL_RETENTION',
          'PORTABLE_QUICK_NATIVE', 'PORTABLE_ALARM_NATIVE', 'PORTABLE_QUICK_RADIOS',
          'PORTABLE_QUICK_ATOMIC_PREFERENCES', 'PORTABLE_ASYNC_RGB565',
          'PORTABLE_FORCE_FULL_FRAMES', 'PORTABLE_WATCH_NATIVE_HEIGHT')
APP_DEFINES = {
    'settings': ('PORTABLE_SETTINGS_NATIVE_UI', 'PORTABLE_SETTINGS_ATOMIC_EXPERIMENTAL',
                 'PORTABLE_SLEEP_SETTINGS', 'PORTABLE_ALARM_SETTINGS'),
    'wifi': ('PORTABLE_WIFI_NATIVE_UI', 'PORTABLE_WATCH_COLOR_ONLY', 'PORTABLE_QUICK_ACTIONS',
             'PORTABLE_HOME_AUTOMATIC_PREFERENCES', 'PORTABLE_WIFI_STORAGE_INSTANCE=6'),
}
CONTROLLERS = {
    'settings': ('PORTABLE_SETTINGS_APP', 'PORTABLE_SETTINGS_ATOMIC_EXPERIMENTAL'),
    'wifi': ('PORTABLE_WIFI_SETTINGS_APP', 'PORTABLE_WIFI_ATOMIC_EXPERIMENTAL',
             'PORTABLE_WIFI_REMEMBER_CONNECTED', 'PORTABLE_SETTINGS_AVAILABILITY',
             'PORTABLE_HOME_AUTOMATIC_PREFERENCES', 'PORTABLE_WIFI_PROFILES',
             'PORTABLE_TEXT_INPUT_CLIENT'),
}
FIXTURES = ('portable_settings_test.c', 'portable_alarm_test.c', 'quick_adapter_test.c',
            'portable_wifi_test.c', 'wifi_workflow_test.c', 'wifi_atomic_workflow_test.c',
            'wifi_availability_test.c')
QUICK_SOURCES = ('quick_actions.c', 'quick_render.c', 'quick_session.c', 'quick_radios.c')
SANITIZERS = ('-fsanitize=address,undefined', '-fno-sanitize-recover=all',
              '-fno-omit-frame-pointer', '-no-pie')
# Audited released identities and bytes are fixed independently of the submitted
# comparison and the repository's human-readable control metadata.
BASELINES = {
    'settings': ('native-settings-1.4.1-dev1', '3050182821b7c4352a5f953c38bb1d32369c67be', {
        'settings-build-record.json': 'edfef775cfbfc86c7cb1310e296de70c72463373d998f4782e6bdf7d192ed015',
        'memory-admission.json': 'b54dabc9f51c4596f4dc10341161a7b3c7e5e77c36a454951a6e53ab51c7371e',
        'command.json': '7952dc87c3a7197903389afc1d1439b66452876a007b0014a2ae4088dc757606',
        'settings.json': '9cc858d0ab11fe4c878c868f1a06cfc4a008702c445170abc5ef3de159239ffa'}),
    'wifi': ('native-wifi-1.4.0-dev1', '9d2c55b32e88c0b12b1fa16200f399164767f5d1', {
        'wifi-build-record.json': '280795394184a14a7b1cb55b8bfe8ad1d892c06590a3462847859d7cdd723a74',
        'elf-memory-admission.json': '104e9666b30a0a90f3743ba9f5eeb544cf88346bade19221d062d7f19905abc5',
        'command.json': '2f9eb4c586edf29cdaf67a4bbfe8410de16e875f6fa4b900a0f7ad3509f809e6',
        'wifi_settings.json': 'e18bd21a1f13f6bb5703ff7f0c75d9663277a8600e932b398d1b764e6241af93'}),
}


def require(condition, message):
    if not condition:
        raise RuntimeError('Native overlay evidence: '+message)


def raw_sha(data):
    return hashlib.sha256(data).hexdigest()


def sha(path):
    return raw_sha(Path(path).read_bytes())


def digest(value):
    require(isinstance(value, str) and re.fullmatch('[0-9a-f]{64}', value), 'invalid SHA-256')
    return value


def read_json(path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, 'duplicate JSON key: '+key)
            result[key] = value
        return result
    return json.loads(Path(path).read_text(), object_pairs_hook=unique)


def relative_path(value):
    path = Path(value)
    require(not path.is_absolute() and '..' not in path.parts and str(path) == value,
            'unsafe relative source path')
    return path


def source_paths(root):
    paths = [root/'test/native_apps'/name for name in FIXTURES]
    paths += [root/'test/native_apps'/('native_overlay_'+name+'_test.c')
              for name in ('settings', 'wifi', 'providers')]
    paths += [root/'scripts'/name for name in ('test_native_overlay_apps.py', 'test_native_settings.py')]
    paths += [p for directory in ('Apps', 'lib/PortableApps', 'lib/NativeApps')
              for p in (root/directory).rglob('*') if p.is_file()]
    return {str(p.relative_to(root)): sha(p) for p in paths}


def _check_comparison(app, artifact, record, source_sha, root):
    control = read_json(root/'docs/native-overlays/released-controls.json')[app]
    tag, source, assets = BASELINES[app]
    require(control['tag'] == tag and control['source_commit'] == source
            and control['assets'] == assets, 'immutable released baseline identity changed')
    for name, expected in assets.items():
        require(sha(root/'docs/native-overlays'/('released-'+app)/name) == expected,
                'immutable released baseline bytes changed: '+name)
    require(isinstance(source_sha, str) and re.fullmatch('[0-9a-f]{40}', source_sha),
            'exact source commit is required')
    require(record['repository_commit'] == source_sha and record['working_tree_dirty'] is False,
            'target source identity changed')
    require(record.get('hardware_verified') is False
            and record.get('firmware_or_product_replacement') is False,
            'target must remain software-only')
    require(record['external_sources']['runtime']['commit'] == RUNTIME_PIN, 'target Runtime pin changed')
    sources = record['source_sha256']
    require(isinstance(sources, dict) and sources, 'target source hashes missing')
    old = read_json(root/'docs/native-overlays'/('released-'+app)/(app+'-build-record.json'))
    require(set(old['source_sha256']) <= set(sources), 'released source closure is incomplete')
    for name, expected in sources.items():
        digest(expected)
        namespace, relative = name.split(':', 1)
        relative_path(relative)
        if namespace == 'system':
            require(sha(root/relative) == expected, 'target source is stale: '+relative)
        elif name in old['source_sha256']:
            require(expected == old['source_sha256'][name], 'pinned dependency bytes changed: '+name)
        else:
            require(namespace in record['external_sources'], 'unknown source namespace: '+namespace)
    # The existing comparison implementation checks exact selectors, capability,
    # ABI, dependency pins, ELF bytes and structural admission against the control.
    require(comparison.ROOT == root, 'comparison and publisher use different checkouts')
    memory = 'memory-admission.json' if app == 'settings' else 'elf-memory-admission.json'
    admission = read_json(artifact/'app'/memory)
    image = admission['images']['native']
    require(admission['runtime_commit'] == RUNTIME_PIN
            and admission['linked_device_memory_fit_qualified'] is False
            and image['section_loader']['within_structural_limit'] is True,
            'target admission is incomplete, unpinned or claims physical fit')
    stem = 'settings' if app == 'settings' else 'wifi_settings'
    elf = (artifact/'app'/(stem+'.elf')).read_bytes()
    manifest = read_json(artifact/'app'/(stem+'.json'))
    require(raw_sha(elf) == record['sha256'] and len(elf) == record['size_bytes']
            and manifest['version'] == record['version']
            and manifest['requires'] == record['required_capabilities'],
            'target ELF or manifest identity changed')
    expected = comparison.compare(app, artifact/'app', artifact/'app'/memory)
    submitted = read_json(artifact/'app/native-overlay-comparison.json')
    require(submitted == expected, 'missing, stale or tampered target comparison')
    require(submitted['hardware_verified'] is False
            and submitted['linked_device_memory_fit_qualified'] is False,
            'comparison claims physical fit')


def _macros(text):
    pairs = re.findall(r'^#define (\w+)(?: (.*))?$', text, re.M)
    require(len(pairs) == len(dict(pairs)), 'duplicate preprocessor macro')
    return dict(pairs)


def _check_receipt(app, directory, sanitized, target, current, root):
    receipt = read_json(directory/'receipt.json')
    require(receipt['status'] == 'PASS' and receipt['hardware_tested'] is False,
            'actual-app receipt is incomplete or claims hardware verification')
    require(receipt['cases'] == CASES and receipt['runtime_pin'] == RUNTIME_PIN,
            'case contract or Runtime pin changed')
    require(receipt['source_sha256'] == current, 'actual-app source closure is missing or stale')
    expected_names = {f'{app}-{w}x{h}-snapshot{s}-san{int(sanitized)}': (w, h, s)
                      for w, h in SIZES for s in (0, 1)}
    builds = receipt['builds']
    require(len(builds) == 4 and {b['name'] for b in builds} == set(expected_names),
            'exact unique four-build actual-app matrix required')
    runs = receipt['runs']
    require(len(runs) == 20 and {(r['build'], r['case']) for r in runs}
            == {(name, case) for name in expected_names for case in range(5)}
            and all(type(r['case']) is int for r in runs),
            'exact unique twenty-run actual-app matrix required')
    # Verify retained staged inputs locally. Never dereference a receipt's
    # historical absolute paths (which can name unrelated or unavailable files).
    maps = {}
    for key, folder, glob in (('runtime_sdk_sha256', 'runtime-sdk', '*.h'),
                              ('staged_include_sha256', 'include', '*.h'),
                              ('fixture_sha256', 'fixtures', '*.c')):
        values = receipt[key]
        require(isinstance(values, dict) and values, 'missing '+key)
        actual = {p.name: sha(p) for p in (directory/folder).glob(glob) if p.is_file()}
        require(values == actual, 'staged input hashes changed: '+folder)
        for name, value in values.items():
            require(relative_path(name).name == name, 'invalid staged filename')
            digest(value)
            maps[folder+'/'+name] = value
    require(set(receipt['fixture_sha256']) == set(FIXTURES), 'staged fixture closure incomplete')
    include = {p.name: sha(p) for p in (root/'lib/PortableApps/include').glob('*.h')}
    for name in ('RiscRuntimeV1.h', 'RiscKeyValueAtomicV1.h'):
        include[name] = receipt['runtime_sdk_sha256'][name]
    require(receipt['staged_include_sha256'] == include, 'staged headers differ from current source')
    for key, value in target['source_sha256'].items():
        if key.startswith('runtime:sdk/app/'):
            require(receipt['runtime_sdk_sha256'].get(Path(key).name) == value,
                    'host Runtime SDK differs from pinned target: '+key)
    # Include directory is staged from the checked source, except the two exact
    # Runtime overrides. Time headers remain byte-for-byte source copies.
    for p in (root/'lib/PortableApps/time').rglob('*'):
        if p.is_file():
            relative = 'time/'+str(p.relative_to(root/'lib/PortableApps/time'))
            require(sha(directory/relative) == sha(p), 'staged time dependency changed')
            maps[relative] = sha(p)

    canonical = {str((root/name).resolve()): name for name in current}
    canonical.update({str((root/name[7:]).resolve()): name[7:]
                      for name in target['source_sha256'] if name.startswith('system:')})
    for build in builds:
        name = build['name']; w, h, snapshot = expected_names[name]
        require(type(build['returncode']) is int and build['returncode'] == 0
                and build['sanitized'] is sanitized and type(build['snapshot']) is int
                and build['snapshot'] == snapshot, 'build mode or status mismatch: '+name)
        digest(build['binary_sha256'])  # Disposable host binaries are deliberately removed.
        require(sha(directory/(name+'-compile.log')) == digest(build['log_sha256']),
                'compile log changed: '+name)
        macro_file = directory/(name+'-macros.txt')
        require(sha(macro_file) == digest(build['macro_sha256']), 'preprocessor log changed: '+name)
        macros = _macros(macro_file.read_text())
        require(build['actual_macros'] == {k: v for k, v in macros.items()
                                         if k.startswith(('PORTABLE_', 'WATCH_'))},
                'recorded macro set differs from preprocessor output')
        chosen = (*COMMON, *APP_DEFINES[app])
        require(build['selected_defines'] == list(chosen), 'full chosen controller selectors missing')
        for define in chosen:
            key, _, value = define.partition('=')
            require(macros.get(key) == (value or '1'), 'selected macro missing or overridden: '+key)
        for key in ('PORTABLE_QUICK_ACTIONS', 'PORTABLE_ALARM_CLIENT', *CONTROLLERS[app]):
            require(key in macros, 'derived controller macro missing: '+key)
        require(('PORTABLE_RASTER_SNAPSHOT' in macros) == bool(snapshot)
                and (not snapshot or macros['PORTABLE_RASTER_SNAPSHOT'] == '1')
                and 'NDEBUG' not in macros, 'snapshot or assertions disabled')
        command = build['command']
        require(isinstance(command, list) and command and all(isinstance(v, str) for v in command),
                'missing host compile command')
        require(command[-2] == '-o' and Path(command[-1]).name == name,
                'host output identity mismatch')
        origin = Path(command[-1]).parent
        require(origin.is_absolute() and '..' not in origin.parts, 'unsafe host output provenance')
        fixture = 'quick_adapter_test.c' if app == 'settings' else 'wifi_availability_test.c'
        flags = ['-D'+d for d in chosen]
        if snapshot:
            flags.append('-DPORTABLE_RASTER_SNAPSHOT')
        flags.append('-DNATIVE_OVERLAY_'+app.upper()+'_FIXTURE='+json.dumps(str(origin/'fixtures'/fixture)))
        if app == 'wifi':
            flags += [f'-DTEST_WIFI_RGB_WIDTH={w}', f'-DTEST_WIFI_RGB_HEIGHT={h}']
            require(macros.get('TEST_WIFI_RGB_WIDTH') == str(w)
                    and macros.get('TEST_WIFI_RGB_HEIGHT') == str(h), 'Wi-Fi dimensions changed')
        if sanitized:
            flags += list(SANITIZERS)
        # Recover the historical checkout path only from the known app-test
        # command input; it is used for string comparison, never file access.
        suffix = '/test/native_apps/native_overlay_'+app+'_test.c'
        test_sources = [s for s in command if s.endswith(suffix)]
        require(len(test_sources) == 1, 'actual app fixture missing from compile command')
        checkout = Path(test_sources[0][:-len(suffix)])
        require(checkout.is_absolute() and '..' not in checkout.parts, 'invalid checkout provenance')
        sources = ([str(checkout/'Apps/settings.c')] if app == 'settings' else [])
        sources += [str(checkout/suffix.lstrip('/'))]
        sources += [str(checkout/'lib/PortableApps/src'/s) for s in QUICK_SOURCES]
        if app == 'wifi':
            sources += [str(checkout/'lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c')]
        prefix = [command[0], '-std=c11', '-O1', '-g0', '-Wall', '-Wextra', '-Werror',
                  '-Wno-unused-function', *flags, '-I'+str(origin/'include'),
                  '-I'+str(origin/'runtime-sdk'), '-I'+str(checkout/'lib/NativeApps/include'),
                  '-I'+str(checkout/'lib/RemoteFiles/vendor/tinydtls_sha2')]
        require(command == [*prefix, *sources, '-o', str(origin/name)],
                'host recipe was weakened or changed')
        require(build['preprocessor_command'] == [*prefix, '-dM', '-E', test_sources[0]],
                'preprocessor recipe differs from compiled recipe')
        dependencies = build['compiler_dependency_sha256']
        require(isinstance(dependencies, dict) and dependencies, 'compiler dependency hashes missing')
        seen = set()
        for path, value in dependencies.items():
            digest(value)
            p = Path(path)
            require(p.is_absolute() and '..' not in p.parts, 'unsafe compiler dependency provenance')
            if p.is_relative_to(origin):
                relative = str(p.relative_to(origin))
                require(relative in maps and maps[relative] == value, 'staged compiler dependency changed: '+relative)
                label = 'staged:'+relative
            else:
                relative = (str(p.relative_to(checkout)) if p.is_relative_to(checkout)
                            else canonical.get(str(p)))
                if relative is None:
                    # A CI checkout may have symlinked inputs whose historical
                    # realpaths differ from today's checkout. A unique known
                    # repository-relative suffix plus its current content hash
                    # binds those inputs without reading that historical path.
                    matches = {r for r in canonical.values() if path.endswith('/'+r)}
                    require(len(matches) == 1, 'unmapped compiler dependency: '+path)
                    relative = matches.pop()
                relative_path(relative)
                require(sha(root/relative) == value, 'compiler dependency is stale: '+relative)
                require(current.get(relative, target['source_sha256'].get('system:'+relative)) == value,
                        'compiler dependency absent from source closure: '+relative)
                if 'system:'+relative in target['source_sha256']:
                    require(target['source_sha256']['system:'+relative] == value,
                            'host compiler dependency differs from target: '+relative)
                label = relative
            require(label not in seen, 'duplicate canonical compiler dependency: '+label)
            seen.add(label)
        critical = {'test/native_apps/native_overlay_'+app+'_test.c',
                    'test/native_apps/native_overlay_providers_test.c',
                    'Apps/settings.c' if app == 'settings' else 'Apps/wifi_settings.c',
                    *('lib/PortableApps/src/'+s for s in QUICK_SOURCES),
                    *('lib/PortableApps/src/'+s for s in ('adapter.c', 'quick_adapter.inc',
                       'quick_native_render.inc', 'quick_atomic.inc', 'alarm.inc', 'alarm_native.inc')),
                    'staged:fixtures/'+fixture, 'staged:include/RiscRuntimeV1.h',
                    'staged:include/RiscKeyValueAtomicV1.h'}
        critical |= ({'lib/PortableApps/src/settings.inc', 'lib/PortableApps/src/settings_native_view.inc',
                      'lib/PortableApps/src/settings_atomic.inc'} if app == 'settings'
                     else {'Apps/wifi_settings_portable.inc', 'Apps/wifi_atomic.inc',
                           'lib/PortableApps/src/wifi_native_view.inc'})
        if snapshot:
            critical |= {'lib/PortableApps/src/raster_snapshot_state.inc',
                         'lib/PortableApps/src/raster_snapshot_replay.inc'}
        require(critical <= seen, 'actual app/controller compiler dependency closure is incomplete')
        for run in [r for r in runs if r['build'] == name]:
            case = run['case']
            expected_command = [str(origin/name), *([str(w), str(h)] if app == 'settings' else []), str(case)]
            require(run['command'] == expected_command and type(run['returncode']) is int
                    and run['returncode'] == 0, 'run failed or dimensions/case changed')
            log = directory/(name+f'-case{case}.log')
            require(sha(log) == digest(run['log_sha256']), 'run log changed')
            title = 'Settings' if app == 'settings' else 'Wi-Fi'
            require(f'{title} native overlays {w}x{h} case{case} PASS:' in log.read_text(),
                    'actual-app passing assertion marker missing')
    return sha(directory/'receipt.json')


def validate_native_overlay_evidence(app, artifact, record, source_sha):
    """Validate all evidence before any release mutation; return publication hashes."""
    try:
        require(app in BASELINES, 'unsupported application')
        artifact = Path(artifact)
        _check_comparison(app, artifact, record, source_sha, ROOT)
        current = source_paths(ROOT)
        hashes = {'comparison_sha256': sha(artifact/'app/native-overlay-comparison.json')}
        for mode, sanitized in (('normal', False), ('sanitized', True)):
            hashes[mode+'_receipt_sha256'] = _check_receipt(
                app, artifact/('overlays-'+mode), sanitized, record, current, ROOT)
        return hashes
    except (OSError, ValueError, KeyError, TypeError, IndexError, AttributeError) as error:
        raise RuntimeError('Native overlay evidence is missing or invalid: '+str(error)) from error


def main():
    """Read-only qualification entry point for PR CI; never invokes GitHub."""
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app', choices=tuple(BASELINES), required=True)
    parser.add_argument('--artifact', type=Path, required=True)
    parser.add_argument('--source-sha', required=True)
    args = parser.parse_args()
    record = read_json(args.artifact/'app'/(args.app+'-build-record.json'))
    print(json.dumps(validate_native_overlay_evidence(
        args.app, args.artifact, record, args.source_sha), sort_keys=True))


if __name__ == '__main__':
    main()

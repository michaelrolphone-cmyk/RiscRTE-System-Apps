#!/usr/bin/env python3
"""Qualify selected X4 update scrolling and exact Watch/X4 flag-off preservation.

All generated files stay under this checkout's build directory. This script does
not alter a product checkout, release, tag, firmware cohort, or published asset.
"""

import argparse
import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASELINE = '7b175418e3063d9f4c571acf06c366144831b2af'
VERSION = '1.2.4'
APPS = ('ota_update', 'app_store')
PROVIDERS = ('software-update-firmware', 'software-update-apps')
IDLE_HEADERS = ('RiscDisplayOutputV1.h', 'RiscDisplayOutputPowerV1.h',
                'RiscTouchV1.h', 'RiscTouchPowerV1.h', 'RiscStorageVolumeV1.h')
SLEEP_HEADERS = ('RiscLightSleepV1.h', 'RiscTimedSleepV1.h', 'RiscDeepSleepV1.h')
REQUIRED_DEFINES = {
    '-DPORTABLE_TOUCH_SCROLL', '-DPORTABLE_APP_TOUCH_SCROLL',
    '-DPORTABLE_UPDATE_TOUCH_SCROLL', '-DPORTABLE_NATIVE_TIME_TOOLBAR',
    '-DPORTABLE_NATIVE_CUSTODY_FENCE', '-DALARM_SERVICE_TAGGED_V2',
    '-DPORTABLE_PAPER_TRANSITIONS', '-DPORTABLE_X4_IDLE_POLICY',
    '-DPORTABLE_LOW_BATTERY', '-DPORTABLE_APP_SLEEP_LOCAL',
    '-DPORTABLE_BLE_BROADCAST', '-DPORTABLE_BLE_BROADCAST_DEFAULT_OFF',
    '-DPORTABLE_PAPER_PREFERENCES', '-DPORTABLE_INPUT_NAVIGATION',
    '-DPORTABLE_DISPLAY_ROTATION=90', '-DPORTABLE_UPDATE_FEED_DISABLED',
}


def check(condition, message):
    if not condition:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_json(path):
    return json.loads(path.read_text())


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')


def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args], text=True).strip()


def artifact_files(name):
    return [name + '.elf', 'manifest.json', name + '.json'] if name in APPS else [
        'driver.elf', 'manifest.json']


def compare(left, right, names):
    result = {}
    for name in names:
        files = {}
        for filename in artifact_files(name):
            a, b = left / name / filename, right / name / filename
            check(a.read_bytes() == b.read_bytes(),
                  f'Exact byte mismatch: {a} != {b}')
            files[filename] = {'sha256': digest(b), 'bytes': b.stat().st_size}
        result[name] = {'exact_bytes': True,
                        'version': read_json(right / name / 'manifest.json')['version'],
                        'files': files}
        print(f'{name}: exact bytes PASS ({right.name})', flush=True)
    return result


def validate_inputs(args):
    records = {}
    for app in APPS:
        path = args.baseline_receipts / app / 'build-record.json'
        record = read_json(path)
        admission = read_json(path.parent / 'x4-native-app.json')
        check(record['repository_commit'] == BASELINE and
              admission['source_revision'] == BASELINE,
              f'{app}: expected baseline source {BASELINE}')
        check(record['version'] == admission['version'] == '1.2.3',
              f'{app}: baseline version is not 1.2.3')
        check(digest(path.parent / (app + '.elf')) ==
              record['sha256'] == admission['elf_sha256'],
              f'{app}: baseline ELF receipt mismatch')
        check(len(record['required_grants']) == 14 and
              record['required_grants'] == admission['required_grants'],
              f'{app}: expected the existing 14-grant cohort')
        records[app] = record
    reference = records[APPS[0]]
    idle = reference['idle_policy']
    args.idle_source = (args.idle_source or Path(idle['source'])).resolve()
    staged_sdk = Path(idle['compiled_include_directory'])
    args.idle_sdk = (args.idle_sdk or staged_sdk).resolve()
    args.idle_runtime_sdk = (args.idle_runtime_sdk or staged_sdk).resolve()
    check(digest(args.idle_source) == idle['helper_sha256'],
          'Idle helper differs from the frozen baseline input')
    for directory, names in ((args.idle_sdk, IDLE_HEADERS),
                             (args.idle_runtime_sdk, SLEEP_HEADERS)):
        for name in names:
            check(digest(directory / name) == idle['sdk_sha256'][name],
                  f'Idle SDK input differs from baseline: {directory / name}')
    for record in records.values():
        check(record['idle_policy']['sdk_sha256'] == idle['sdk_sha256'] and
              record['idle_policy']['helper_sha256'] == idle['helper_sha256'],
              'OTA Update and App Store baseline idle inputs differ')
    for repo, field, source in ((args.runtime, 'native_time_sdk', 'sdk/app/'),
                                (args.utilities, 'tagged_alarm_sdk', 'lib/Alarm/include/')):
        sdk = reference[field]
        for name, expected in sdk['sha256'].items():
            path = 'LICENSE' if name == 'LICENSE' else source + name
            blob = subprocess.check_output([
                'git', '-C', str(repo), 'show', sdk['commit'] + ':' + path])
            check(hashlib.sha256(blob).hexdigest() == expected,
                  f'Pinned input differs from baseline: {field}/{name}')
    compiler = subprocess.check_output([str(args.compiler), '--version'], text=True).splitlines()[0]
    check(compiler == reference['compiler'] and '8.4.0' in compiler,
          'Qualification requires the same GCC 8.4.0 toolchain as the baseline')
    return records, compiler


def build(repo, flags, destination, args, commands):
    check(not destination.exists(), f'Refusing to reuse build output: {destination}')
    destination.mkdir(parents=True)
    command = [sys.executable, str(repo / 'scripts/build_portable_updates.py'),
               *flags, '--output-dir', str(destination)]
    env = dict(os.environ, NATIVE_APP_CC=str(args.compiler),
               PYTHONDONTWRITEBYTECODE='1', TMPDIR=str(args.output_dir / 'tmp'))
    log = args.output_dir / (destination.name + '.log')
    with log.open('w') as output:
        result = subprocess.run(command, cwd=repo, env=env, stdout=output,
                                stderr=subprocess.STDOUT, timeout=600)
    commands.append({'command': command, 'log': str(log), 'returncode': result.returncode})
    write_json(args.output_dir / (args.phase + '-commands.json'), commands)
    check(result.returncode == 0,
          f'Build failed ({result.returncode}); see {log}\n{log.read_text()[-6000:]}')
    return destination


def baseline_checkout(out):
    path = out / 'baseline-source'
    if path.exists():
        check(git(path, 'rev-parse', 'HEAD') == BASELINE and
              not git(path, 'status', '--porcelain'),
              'Existing baseline checkout is not the clean pinned commit')
    else:
        subprocess.run(['git', 'clone', '--shared', '--no-checkout', '--quiet',
                        str(ROOT), str(path)], check=True)
        subprocess.run(['git', '-C', str(path), 'checkout', '--detach', '--quiet', BASELINE],
                       check=True)
    return path


def qualify_selected(selected, baseline_records):
    result = {}
    for app in APPS:
        path = selected / app
        manifest = read_json(path / 'manifest.json')
        check(manifest == read_json(path / (app + '.json')),
              f'{app}: duplicated manifests differ')
        record = read_json(path / 'build-record.json')
        admission = read_json(path / 'x4-native-app.json')
        elf = path / (app + '.elf')
        check(manifest['version'] == record['version'] == admission['version'] == VERSION,
              f'{app}: selected app must be version {VERSION}')
        check(digest(elf) == record['sha256'] == admission['elf_sha256'],
              f'{app}: ELF hash mismatch')
        check(elf.stat().st_size == record['size_bytes'] == admission['elf_bytes'],
              f'{app}: ELF size mismatch')
        check(record['build_defines'] == admission['build_defines'] and
              REQUIRED_DEFINES <= set(record['build_defines']),
              f'{app}: selected build/admission flags mismatch')
        scrolling = record.get('touch_scrolling', {})
        check(admission.get('touch_scrolling') == scrolling and
              scrolling.get('version') == 1 and
              all(scrolling.get(field) is True for field in
                  ('bounded_viewport', 'momentum', 'completed_frame_identity',
                   'confirmation_revalidated')),
              f'{app}: touch scrolling identity/momentum receipt mismatch')
        baseline = baseline_records[app]
        check(record['required_grants'] == admission['required_grants'] == baseline['required_grants'],
              f'{app}: scrolling must preserve the baseline 14 grants')
        check(manifest['requires'] == admission['requires'] == baseline['requested_capabilities'],
              f'{app}: scrolling must preserve capability requirements')
        check(record['native_time_sdk'] == baseline['native_time_sdk'] and
              record['tagged_alarm_sdk'] == baseline['tagged_alarm_sdk'],
              f'{app}: native time or tagged alarm SDK differs')
        check(record['idle_policy'] == admission['idle_policy'] and
              record['idle_policy']['helper_sha256'] == baseline['idle_policy']['helper_sha256'] and
              record['idle_policy']['sdk_sha256'] == baseline['idle_policy']['sdk_sha256'],
              f'{app}: idle receipt or SDK differs')
        expected_sdk = dict(record['native_time_sdk']['sha256'],
                            **record['tagged_alarm_sdk']['sha256'],
                            **record['idle_policy']['sdk_sha256'])
        expected_sdk.pop('LICENSE')
        check(admission['sdk_sha256'] == expected_sdk,
              f'{app}: admission SDK hashes do not match recorded inputs')
        check(record['ble_broadcast'] == admission['ble_broadcast'] and
              record['ble_broadcast']['default'] == 'off',
              f'{app}: BLE receipt mismatch')
        check(record['update_policy'] == baseline['update_policy'] and
              not record['update_policy']['feed_configured'],
              f'{app}: product update policy changed')
        check(record['exports'] == ['app_main', 'app_module_fini', 'app_module_init'],
              f'{app}: app ABI exports changed')
        for name in ('Apps/update_scroll.inc', 'Apps/update_portable.inc',
                     'Apps/update_paper.inc', 'scripts/build_portable_updates.py',
                     'scripts/portable_idle_build.py',
                     'lib/PortableApps/include/PortableTouchScroll.h',
                     'lib/PortableApps/src/adapter.c'):
            check(record['source_sha256'].get(name) == digest(ROOT / name),
                  f'{app}: selected source is missing or stale in receipt: {name}')
        for name, expected in record['source_sha256'].items():
            source = ROOT / name
            if source.is_file():
                check(digest(source) == expected, f'{app}: source changed during build: {name}')
        check(record['sha256'] != baseline['sha256'],
              f'{app}: scrolling selection did not change the app ELF')
        result[app] = {'version': VERSION, 'sha256': digest(elf),
                       'bytes': elf.stat().st_size, 'grant_count': 14,
                       'build_record': str(path / 'build-record.json'),
                       'admission_receipt': str(path / 'x4-native-app.json'),
                       'working_tree_dirty': record['working_tree_dirty']}
        print(f'{app}: selected {VERSION}, 14 grants and receipts PASS', flush=True)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--utilities', type=Path, required=True)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--baseline-receipts', type=Path, required=True,
                        help='Existing 1.2.3 updates directory; read only')
    parser.add_argument('--idle-source', type=Path)
    parser.add_argument('--idle-sdk', type=Path)
    parser.add_argument('--idle-runtime-sdk', type=Path)
    parser.add_argument('--phase', choices=('plan', 'flag-off', 'selected', 'all'), default='all')
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'build/update-touch-scroll')
    args = parser.parse_args()
    for name in ('runtime', 'utilities', 'compiler', 'baseline_receipts', 'output_dir'):
        setattr(args, name, getattr(args, name).resolve())
    check(args.output_dir.is_relative_to((ROOT / 'build').resolve()) and
          args.output_dir != (ROOT / 'build').resolve(),
          '--output-dir must be a child of this checkout build directory')
    records, compiler = validate_inputs(args)
    native = ['--product', 'x4', '--time-profile', 'x4-native-time',
              '--native-time-runtime-repo', str(args.runtime),
              '--tagged-alarm-utilities', str(args.utilities), '--alarm-client',
              '--quick-actions', '--quick-radios', '--paper-transitions', '--stage-logs',
              '--navigation', '--display-rotation', '90', '--wifi-instance', '15',
              '--home-app', 'default.elf', '--ble-broadcast',
              '--x4-idle-source', str(args.idle_source),
              '--x4-idle-sdk', str(args.idle_sdk),
              '--x4-idle-runtime-sdk', str(args.idle_runtime_sdk)]
    watch = ['--nova-ui', '--alarm-client', '--quick-actions', '--quick-radios',
             '--home-app', 'default.elf', '--wifi-instance', '15',
             '--rtc-utc-offset-seconds', '28800']
    args.output_dir.mkdir(parents=True, exist_ok=True)
    (args.output_dir / 'tmp').mkdir(exist_ok=True)
    report = {'schema': 1, 'baseline_source': BASELINE,
              'source_commit': git(ROOT, 'rev-parse', 'HEAD'),
              'compiler': compiler, 'phase': args.phase, 'hardware_verified': False,
              'publication': 'none',
              'preservation_scope': 'Builder flag-off artifacts from the pinned baseline only; '
                                    'not a qualification of the active Watch release',
              'baseline_receipts': str(args.baseline_receipts),
              'baseline_receipt_sha256': {app: digest(args.baseline_receipts / app / 'build-record.json')
                                         for app in APPS},
              'flags': {'watch': watch, 'x4': native,
                        'selected': [*native, '--touch-scrolling']}}
    commands = []
    if args.phase in ('flag-off', 'all'):
        baseline = baseline_checkout(args.output_dir)
        for product, flags in (('watch', watch), ('x4', native)):
            old = build(baseline, flags, args.output_dir / (product + '-baseline'), args, commands)
            new = build(ROOT, flags, args.output_dir / (product + '-flag-off'), args, commands)
            report[product + '_flag_off'] = compare(old, new, (*APPS, *PROVIDERS))
            if product == 'x4':
                report['frozen_receipt_bytes'] = compare(args.baseline_receipts, old, (*APPS, *PROVIDERS))
    if args.phase in ('selected', 'all'):
        selected = build(ROOT, [*native, '--touch-scrolling'],
                         args.output_dir / 'selected', args, commands)
        report['selected'] = qualify_selected(selected, records)
        report['selected_provider_preservation'] = compare(args.baseline_receipts, selected, PROVIDERS)
    report['commands'] = commands
    report['status'] = 'PLAN_ONLY' if args.phase == 'plan' else 'PASS'
    path = args.output_dir / (args.phase + '-qualification.json')
    write_json(path, report)
    print(str(path), flush=True)


if __name__ == '__main__':
    main()

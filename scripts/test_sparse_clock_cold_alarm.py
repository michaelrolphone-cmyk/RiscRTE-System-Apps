#!/usr/bin/env python3
"""Full-wipe cold boot: real sparse Clock and pinned native-UTC alarm provider.

Proves the delivered source fails before checking the current source. Hardware,
Runtime custody/Graph and electrical RTC behavior remain outside this host test.
"""
import argparse
import hashlib
import io
import json
import os
import shutil
import subprocess
import tarfile
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DELIVERED = 'd305e6b718f51d42ffcf6339e05ce4b56398f68c'
UTILITIES = '637e13b0bce62ad49b756bec2468a6271d163fc7'
X4 = 'e2281ade9eab04e8252c9861cda01a228d613859'
RUNTIME = '7fa39e01d7214a4a341032a15ab467a58ae22cec'
APP_FLAGS = ['TEST_NATIVE_LANDSCAPE', 'PORTABLE_DISPLAY_ROTATION=90',
             'PORTABLE_APP_OWNS_TOUCH_CHROME', 'PORTABLE_RTC_WALL_TIME',
             'PORTABLE_ALARM_CLIENT', 'PORTABLE_INPUT_NAVIGATION',
             'PORTABLE_APP_SLEEP_LOCAL', 'PORTABLE_CROWN_SLEEP_LOCAL',
             'PORTABLE_SLEEP_MANUAL_ONLY', 'PORTABLE_DESK_CLOCK',
             'PORTABLE_DESK_CLOCK_SPARSE_START', 'ALARM_SERVICE_TAGGED_V2']
PROVIDER_FLAGS = ['ALARM_SERVICE_TAGGED_V2', 'ALARM_NATIVE_UTC',
                  'ALARM_VISUAL_ONLY', 'ALARM_DND_CONTROL', 'POINTS_IN_TIME_SERVICE']
SOURCES = ['adapter.c', 'desk_clock_faces.c', 'PortableRealtimeClient.c',
           'PortableTimeZone.c', 'PortableTimeZoneCatalog.c', 'PortableTimeZonePreference.c']
QUICK = ['quick_actions.c', 'quick_render.c', 'quick_session.c', 'quick_radios.c']
DRIVER_HEADERS = ['RiscDisplayOutputV1.h', 'RiscDisplayOutputPowerV1.h',
                  'RiscTouchV1.h', 'RiscTouchPowerV1.h', 'RiscStorageVolumeV1.h']
CASES = ['empty', 'reset-empty', 'missing-zone', 'missing-basis', 'loaded',
         'native-valid', 'bad-zone', 'bad-basis', 'unreadable-zone', 'unreadable-basis',
         'fold', 'gap', 'missing-basis-fold', 'missing-basis-gap', 'invalid-rtc',
         'seed-error', 'rtc-read-error', 'rtc-acquire-error', 'rtc-release-error',
         'seed-context', 'native-release-error', 'alarm-native-context', 'alarm-storage-context']


def run(args, **kwargs):
    return subprocess.run(list(map(str, args)), check=True, **kwargs)


def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args])


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def archive(repo, ref, destination, *paths):
    destination.mkdir(parents=True, exist_ok=True)
    with tarfile.open(fileobj=io.BytesIO(git(repo, 'archive', ref, *paths))) as source:
        source.extractall(destination, filter='data')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--system', type=Path, default=ROOT, help='Current System source under test')
    for name in ('utilities', 'x4', 'runtime', 'sdk'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--evidence', type=Path)
    args = parser.parse_args()
    system = args.system.resolve()
    rows = []
    with tempfile.TemporaryDirectory(prefix='cold-clock-alarm-') as directory:
        stage = Path(directory)
        baseline, utilities, x4, runtime = [stage / name for name in ('delivered', 'utilities', 'x4', 'runtime')]
        archive(system, DELIVERED, baseline)
        archive(args.utilities, UTILITIES, utilities, 'Services/alarm_service/service.c', 'lib/Alarm/include')
        archive(args.x4, X4, x4, 'minimal/apps', 'minimal/drivers/x4pro_power')
        archive(args.runtime, RUNTIME, runtime, 'sdk')
        cc = os.environ.get('CC', 'cc')
        for sanitized in (False, True):
            for quick in (False, True):
                error_frames = {}
                for label, source_root in (('delivered', baseline), ('current', system)):
                    build = stage / f'{label}-{sanitized}-{quick}'
                    includes = build / 'include'
                    shutil.copytree(source_root / 'lib/PortableApps/include', includes)
                    shutil.copytree(source_root / 'lib/PortableApps/time', build / 'time')
                    for folder in (runtime / 'sdk/app', runtime / 'sdk/driver', utilities / 'lib/Alarm/include'):
                        for header in folder.glob('*.h'):
                            shutil.copyfile(header, includes / header.name)
                    for name in DRIVER_HEADERS:
                        shutil.copyfile(args.sdk / name, includes / name)
                    flags = ['-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror']
                    if sanitized:
                        flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie']
                    include_flags = ['-I' + str(includes), '-I' + str(source_root / 'lib/NativeApps/include'),
                                     '-I' + str(x4 / 'minimal/drivers/x4pro_power')]
                    provider = build / 'alarm-provider.o'
                    run([cc, *flags, *include_flags, *['-D' + name for name in PROVIDER_FLAGS],
                         '-c', utilities / 'Services/alarm_service/service.c', '-o', provider])
                    sources = [source_root / 'Apps/paper_clock.c',
                               *[source_root / 'lib/PortableApps/src' / name for name in SOURCES],
                               x4 / 'minimal/apps/portable_sleep.c', ROOT / 'test/native_apps/sparse_clock_cold_alarm_test.c']
                    app_flags = list(APP_FLAGS)
                    if quick:
                        app_flags += ['PORTABLE_QUICK_ACTIONS', 'PORTABLE_QUICK_RADIOS']
                        sources += [source_root / 'lib/PortableApps/src' / name for name in QUICK]
                    binary = build / 'test'
                    run([cc, *flags, *include_flags, *['-D' + name for name in app_flags], *sources, provider, '-o', binary])
                    selected = ['empty', 'reset-empty', 'missing-zone', 'missing-basis'] if label == 'delivered' else CASES
                    for case in selected:
                        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0')
                        if label == 'delivered':
                            env['EXPECT_DELIVERED_FAILURE'] = '1'
                        if (case == 'empty' and label == 'delivered') or (case == 'bad-basis' and label == 'current'):
                            capture = build / 'rtc-error.pixels'
                            env['ALARM_ERROR_FRAME'] = str(capture)
                            error_frames[label] = capture
                        try:
                            result = run([binary, case], env=env, capture_output=True, text=True, timeout=15)
                        except subprocess.CalledProcessError as error:
                            print(error.stdout, end='')
                            print(error.stderr, end='')
                            raise
                        print(result.stdout, end='', flush=True)
                        rows.append({'source': label, 'sanitized': sanitized, 'quick': quick, 'case': case, 'result': result.stdout.strip()})
                assert error_frames['delivered'].read_bytes() == error_frames['current'].read_bytes(), 'Delivered cold failure must render the actual RTC error modal'
        receipt = {'purpose': 'production Clock/controller/adapter/X4 client plus actual native-UTC alarm provider; host boundary doubles',
                   'delivered_system': DELIVERED, 'current_system': git(system, 'rev-parse', 'HEAD').decode().strip(),
                   'current_system_dirty': bool(git(system, 'status', '--porcelain', '--untracked-files=no').strip()),
                   'utilities': UTILITIES, 'x4': X4, 'runtime_sdk': RUNTIME,
                   'driver_headers': {name: sha(args.sdk / name) for name in DRIVER_HEADERS},
                   'provider_sha256': sha(utilities / 'Services/alarm_service/service.c'),
                   'clock_sha256': sha(system / 'Apps/paper_sparse_clock.inc'),
                   'system_sources': {str(path.relative_to(system)): sha(path) for path in [
                       system / 'Apps/paper_clock.c', system / 'Apps/paper_sparse_clock.inc',
                       *[system / 'lib/PortableApps/src' / name for name in SOURCES + QUICK],
                       system / 'lib/PortableApps/src/sparse_clock_adapter.inc',
                       system / 'lib/PortableApps/src/alarm.inc']},
                   'clock_recovery_KV_and_RTC_writes': 0,
                   'provider_ledger': 'Existing factory schedule may compact expired events into points_utc_occ after recovery; no policy repair',
                   'test_sha256': sha(ROOT / 'test/native_apps/sparse_clock_cold_alarm_test.c'),
                   'process_cases': len(rows), 'hardware': 'not run', 'runtime_graph_custody': 'not qualified by this host fixture', 'cases': rows}
        if args.evidence:
            args.evidence.parent.mkdir(parents=True, exist_ok=True)
            args.evidence.write_text(json.dumps(receipt, indent=2) + '\n')
    print(f'{len(rows)} real-provider cold-boot cases passed across normal/ASan+UBSan and Quick off/on')


if __name__ == '__main__':
    main()

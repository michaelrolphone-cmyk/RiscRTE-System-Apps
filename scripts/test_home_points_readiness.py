#!/usr/bin/env python3
"""Run real Home + adapter + selected service readiness with physical providers."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--candidate', type=Path, required=True)
parser.add_argument('--utilities', type=Path, required=True)
parser.add_argument('--runtime', type=Path, required=True)
parser.add_argument('--sleep-source', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--normal-only', action='store_true')
parser.add_argument('--baseline', type=Path, help='Optional clean pre-fix System source tree')
args = parser.parse_args()
out = args.output.resolve()
include = out / 'include'
include.mkdir(parents=True, exist_ok=True)
candidate = json.loads((args.candidate / 'build-evidence.json').read_text())
shutil.copytree(candidate['paper_transition']['compiled_include_directory'], include, dirs_exist_ok=True)
# Preserve the qualified physical SDK, but never compile stale app/catalog
# headers from its earlier build instead of the selected production sources.
for source in (ROOT / 'lib/PortableApps/include').iterdir():
    if source.is_file() and not source.name.startswith('Risc'):
        shutil.copyfile(source, include / source.name)
shutil.copytree(args.utilities / 'lib/Alarm/include', include, dirs_exist_ok=True)
shutil.copytree(ROOT / 'lib/PortableApps/time', out / 'time', dirs_exist_ok=True)
for name in ('RiscAppDataV1.h',):
    shutil.copyfile(args.runtime / 'sdk/app' / name, include / name)
for name in ('RiscBoundAppDataV1.h', 'RiscProviderV2.h', 'RiscPlatformClockV1.h',
             'RiscPlatformRealtimeV1.h', 'RiscBoundKeyValueV1.h'):
    shutil.copyfile(args.runtime / 'sdk/driver' / name, include / name)
flags = '''TEST_NATIVE_LANDSCAPE PORTABLE_APP_OWNS_TOUCH_CHROME PORTABLE_RTC_WALL_TIME
PORTABLE_ALARM_CLIENT ALARM_SERVICE_TAGGED_V2 PORTABLE_ALARM_TERMINAL_RETENTION
PORTABLE_INPUT_NAVIGATION PORTABLE_APP_SLEEP_LOCAL PORTABLE_CROWN_SLEEP_LOCAL
PORTABLE_SLEEP_MANUAL_ONLY PORTABLE_DESK_CLOCK PORTABLE_DESK_CLOCK_SPARSE_START
PORTABLE_DESK_LOCK_HOME PORTABLE_DESK_POINTS_FACE PORTABLE_DESK_POINTS_SNAPSHOT
PORTABLE_HOME_POINTS_NATIVE_UTC ALARM_NATIVE_UTC'''.split()
flags = ['-D' + flag for flag in flags] + ['-DPORTABLE_DISPLAY_ROTATION=90']
incs = ['-I' + str(path) for path in (include, ROOT / 'lib/NativeApps/include',
        args.sleep_source.parent.parent / 'drivers/x4pro_power')]
sources = [ROOT / 'Apps/paper_clock.c', *[ROOT / 'lib/PortableApps/src' / name for name in
    ('adapter.c', 'desk_clock_faces.c', 'PortableRealtimeClient.c', 'PortableTimeZone.c',
     'PortableTimeZoneCatalog.c', 'PortableTimeZonePreference.c')], args.sleep_source,
    ROOT / 'test/native_apps/home_points_readiness_test.c']
service = args.utilities / 'Services/alarm_service/service.c'
cases = ['home-fresh', 'home-empty', 'home-delayed-time', 'home-inflight',
         'home-generation', 'home-catalog-change', 'home-wrap', 'home-storage-error',
         'home-rtc-error', 'home-retained']
runs = []
for sanitized in ([False] if args.normal_only else [False, True]):
    label = 'asan-ubsan' if sanitized else 'normal'
    directory = out / label
    directory.mkdir(exist_ok=True)
    extra = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
             '-fno-omit-frame-pointer', '-no-pie'] if sanitized else []
    common = [os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', *extra, *flags, *incs]
    obj = directory / 'actual-service.o'
    subprocess.run([*common, '-DPOINTS_CATALOG_SERVICE', '-DPOINTS_IN_TIME_SERVICE',
                    '-DALARM_DND_CONTROL', '-DALARM_VISUAL_ONLY', '-c', str(service), '-o', str(obj)], check=True)
    binary = directory / 'home-points-readiness'
    subprocess.run([*common, *map(str, sources), str(obj),
                    '-Wl,--wrap=portable_home_points_catalog', '-o', str(binary)], check=True)
    for case in cases:
        result = subprocess.run([str(binary), case, str(directory / case)], capture_output=True,
            text=True, timeout=20, env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0',
                                      'UBSAN_OPTIONS': 'halt_on_error=1:print_stacktrace=1'})
        (directory / (case + '.log')).write_text(result.stdout + result.stderr)
        assert result.returncode == 0, (label, case, result.returncode, result.stdout, result.stderr)
        print(label + ': ' + result.stdout.strip(), flush=True)
        runs.append({'profile': label, 'case': case, 'result': result.stdout.strip()})
    # First reconciliation must look like loading, not the genuine service
    # storage/RTC failure. Successful and explicitly empty results also differ.
    read = lambda case, frame: (directory / f'{case}.{frame}.pbm').read_bytes()
    assert read('home-fresh', 'first') != read('home-storage-error', 'first')
    assert read('home-fresh', 'first') != read('home-rtc-error', 'first')
    assert read('home-fresh', 'last') != read('home-empty', 'last')
    assert read('home-fresh', 'last') == read('home-generation', 'last')
    assert read('home-fresh', 'last') != read('home-catalog-change', 'last')
if not args.normal_only:
    for path in (out / 'normal').glob('*.pbm'):
        assert path.read_bytes() == (out / 'asan-ubsan' / path.name).read_bytes(), path
baseline_runs = []
if args.baseline:
    directory = out / 'baseline'
    directory.mkdir(exist_ok=True)
    baseline_sources = [args.baseline / path.relative_to(ROOT)
                       if path.is_relative_to(ROOT) and path.name != 'home_points_readiness_test.c'
                       else path for path in sources]
    binary = directory / 'home-points-readiness'
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                    *flags, *incs, '-DREADINESS_EXPECT_BASELINE', *map(str, baseline_sources),
                    str(out / 'normal/actual-service.o'), '-Wl,--wrap=portable_home_points_catalog',
                    '-o', str(binary)], check=True)
    for case in ('home-fresh', 'home-inflight'):
        result = subprocess.run([str(binary), case, str(directory / case)], capture_output=True,
                                text=True, timeout=20)
        (directory / (case + '.log')).write_text(result.stdout + result.stderr)
        assert result.returncode == 42, (case, result.returncode, result.stdout, result.stderr)
        print('baseline: ' + result.stdout.strip(), flush=True)
        baseline_runs.append({'case': case, 'returncode': result.returncode, 'result': result.stdout.strip()})
tracked = [*sources, service, service.with_name('catalog.inc'), Path(__file__),
           ROOT / 'Apps/paper_home_points.inc', ROOT / 'lib/PortableApps/src/home_points_catalog_adapter.inc',
           ROOT / 'test/native_apps/sparse_clock_startup_test.c',
           args.utilities / 'lib/Alarm/include/PointsRecords.h',
           args.utilities / 'lib/Alarm/include/PointsCatalog.h']
(out / 'receipt.json').write_text(json.dumps({'runs': runs,
    'implementation': 'actual paper_clock.c, adapter.c, selected service.c and production catalog.inc',
    'fixtures': 'physical storage/realtime/display/input/Runtime only; the in-flight case advances the actual service from a second admitted test client',
    'fixture_limits': 'host test; does not establish real Runtime scheduling, capability custody, physical display timing, or hardware readiness',
    'utilities_commit': subprocess.check_output(['git', '-C', str(args.utilities), 'rev-parse', 'HEAD'], text=True).strip(),
    'utilities_dirty': bool(subprocess.check_output(['git', '-C', str(args.utilities), 'status', '--porcelain'], text=True)),
    'baseline': {'source': str(args.baseline) if args.baseline else None, 'runs': baseline_runs},
    'source_sha256': {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in tracked},
    'hardware': 'not run'}, indent=2) + '\n')
print(f'{len(runs)} real Home/adapter/service readiness cases PASS')

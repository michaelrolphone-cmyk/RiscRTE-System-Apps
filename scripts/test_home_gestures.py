#!/usr/bin/env python3
"""Exercise actual sparse Home and adapter gestures with strict host providers.

Uses an existing candidate only for pinned SDK headers and the sleep-source
identity. Production code is compiled from this checkout, not the candidate.
No hardware, product image, or remote publication is involved.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--candidate', type=Path, required=True)
    p.add_argument('--sleep-source', type=Path, required=True)
    p.add_argument('--output-dir', type=Path, default=ROOT / 'build/home-gestures')
    p.add_argument('--normal-only', action='store_true')
    p.add_argument('--catalog', action='store_true', help='Use the current service-backed Home Points projection')
    args = p.parse_args()
    out = args.output_dir.resolve(); out.mkdir(parents=True, exist_ok=True)
    receipt = json.loads((args.candidate / 'build-evidence.json').read_text())
    assert sha(args.sleep_source) == receipt['local_sleep_source_sha256']
    include = out / 'include'
    shutil.copytree(receipt['paper_transition']['compiled_include_directory'], include, dirs_exist_ok=True)
    for header in (ROOT / 'lib/PortableApps/include').glob('*.h'):
        if not header.name.startswith(('Risc', 'AlarmService')):
            shutil.copyfile(header, include / header.name)
    shutil.copytree(ROOT / 'lib/PortableApps/time', out / 'time', dirs_exist_ok=True)
    flags = '''TEST_NATIVE_LANDSCAPE PORTABLE_DISPLAY_ROTATION=90 PORTABLE_APP_OWNS_TOUCH_CHROME
PORTABLE_RTC_WALL_TIME PORTABLE_ALARM_CLIENT ALARM_SERVICE_TAGGED_V2 PORTABLE_INPUT_NAVIGATION
PORTABLE_APP_SLEEP_LOCAL PORTABLE_CROWN_SLEEP_LOCAL PORTABLE_SLEEP_MANUAL_ONLY PORTABLE_DESK_CLOCK
PORTABLE_DESK_CLOCK_SPARSE_START PORTABLE_HOME_POINTS_NATIVE_UTC ALARM_NATIVE_UTC'''.split()
    if args.catalog:
        flags += ['PORTABLE_DESK_POINTS_SNAPSHOT', 'PORTABLE_ALARM_TERMINAL_RETENTION']
    sources = [ROOT / 'Apps/paper_clock.c', ROOT / 'test/native_apps/home_gesture_adapter.c',
               ROOT / 'test/native_apps/home_gesture_test.c', args.sleep_source,
               *[ROOT / 'lib/PortableApps/src' / name for name in
                 ('desk_clock_faces.c', 'PortableRealtimeClient.c', 'PortableTimeZone.c',
                  'PortableTimeZoneCatalog.c', 'PortableTimeZonePreference.c')]]
    quick_sources = [ROOT / 'lib/PortableApps/src' / name for name in
                     ('quick_actions.c', 'quick_render.c', 'quick_session.c', 'quick_radios.c')]
    cases = [('time', 240, 160, 0, 0, 'none'), ('tap', 240, 400, 0, 0, 'points_in_time.elf'),
             ('held', 240, 400, 60, 0, 'none'), ('cancel', 240, 400, 60, 0, 'none'),
             ('short', 240, 400, 0, 0, 'none'), ('return', 240, 400, 0, 0, 'none'),
             ('repeat', 240, 160, 0, 0, 'points_in_time.elf')]
    for x, y in ((240, 160), (240, 400), (12, 400), (84, 680), (240, 30)):
        for dx, dy in ((80, 0), (-80, 0), (0, -100), (0, 100), (30, 100), (100, 30)):
            if 0 <= x + dx < 480 and 0 <= y + dy < 800:
                cases.append(('swipe', x, y, dx, dy, 'quick' if dy > abs(dx) else 'springboard.elf'))
    cases += [('release', 240, 400, 80, 0, 'springboard.elf'),
              ('release', 240, 400, 0, 100, 'quick'),
              ('quick-return', 240, 400, 0, 100, 'points_in_time.elf')]
    production = [ROOT / 'Apps/paper_clock.c', *ROOT.glob('Apps/*.inc'), *ROOT.glob('Apps/*.h'),
                  *[p for directory in ('lib/PortableApps', 'lib/NativeApps/include')
                    for p in (ROOT / directory).rglob('*') if p.is_file()]]
    source_hashes = {str(p): sha(p) for p in [*production, *sources, *quick_sources, Path(__file__),
                                            ROOT / 'test/native_apps/sparse_clock_startup_test.c',
                                            ROOT / 'test/native_apps/paper_clock_test.c']}
    sdk_hashes = {str(p.relative_to(include)): sha(p) for p in include.rglob('*') if p.is_file()}
    time_hashes = {str(p.relative_to(out / 'time')): sha(p) for p in (out / 'time').rglob('*') if p.is_file()}
    runs = []
    for quick in (False, True):
        selected = [c for c in cases if quick or c[0] != 'quick-return']
        selected = [(*c[:-1], 'springboard.elf' if c[-1] == 'quick' and not quick else c[-1]) for c in selected]
        for san in ([False] if args.normal_only else [False, True]):
            label = ('quick' if quick else 'plain') + ('-sanitized' if san else '-normal')
            binary = out / label
            extra = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie'] if san else []
            defines = flags + (['PORTABLE_QUICK_ACTIONS', 'PORTABLE_QUICK_RADIOS'] if quick else [])
            subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                            *extra, *['-D' + f for f in defines], '-I' + str(include),
                            '-I' + str(ROOT / 'lib/NativeApps/include'),
                            '-I' + str(args.sleep_source.parent.parent / 'drivers/x4pro_power'),
                            *map(str, sources + (quick_sources if quick else [])), '-o', str(binary)], check=True)
            for asynchronous in (False, True):
                env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
                env.pop('RAW_ASYNC', None)
                if asynchronous: env['RAW_ASYNC'] = '1'
                for index, case in enumerate(selected):
                    result = subprocess.run([str(binary), *map(str, case)], capture_output=True, text=True, env=env, timeout=20)
                    log = out / f'{label}-{int(asynchronous)}-{index}.log'
                    log.write_text(result.stdout + result.stderr)
                    assert result.returncode == 0, (case, label, asynchronous, result.returncode, result.stdout, result.stderr)
                    runs.append({'profile': label, 'async': asynchronous, 'case': case, 'result': result.stdout.strip()})
            print(label + ': ' + str(len(selected) * 2) + ' gesture cases PASS', flush=True)
    assert all(sha(Path(p)) == digest for p, digest in source_hashes.items()), 'Source changed during matrix'
    assert all(sha(include / p) == digest for p, digest in sdk_hashes.items()), 'SDK changed during matrix'
    assert all(sha(out / 'time' / p) == digest for p, digest in time_hashes.items()), 'Compiled time data changed during matrix'
    (out / 'evidence.json').write_text(json.dumps({'runs': runs, 'catalog_projection': args.catalog, 'hardware_tested': False,
        'runtime_mocked': True, 'production_source_sha256': {str(p.relative_to(ROOT)): sha(p) for p in production},
        'source_sha256': source_hashes, 'sdk_sha256': sdk_hashes, 'compiled_time_sha256': time_hashes}, indent=2) + '\n')
    print(str(len(runs)) + ' exact-checkout Home gesture cases PASS: ' + str(out / 'evidence.json'))

if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Production sparse adapter cold-KV and alarm/frame liveness regression.

Use a candidate built with build_paper_clock.py for its selected idle SDK.
An optional unmodified source directory runs the failing-before controls.
No device or physical storage access is performed.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--candidate', type=Path, required=True)
    p.add_argument('--baseline-system', type=Path)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    out = a.output.resolve(); out.mkdir(parents=True, exist_ok=True)
    inc = out / 'include'
    shutil.copytree(a.candidate / 'idle-sdk/include', inc, dirs_exist_ok=True)
    for h in (ROOT / 'lib/PortableApps/include').glob('*.h'):
        if not h.name.startswith(('Risc', 'AlarmService')):
            shutil.copyfile(h, inc / h.name)
    shutil.copytree(ROOT / 'lib/PortableApps/time', out / 'time', dirs_exist_ok=True)
    defines = '''PORTABLE_DESK_CLOCK PORTABLE_DESK_CLOCK_SPARSE_START PORTABLE_ALARM_CLIENT
    PORTABLE_APP_SLEEP_LOCAL PORTABLE_INPUT_NAVIGATION PORTABLE_DISPLAY_ROTATION=90
    PORTABLE_RTC_WALL_TIME PORTABLE_APP_OWNS_TOUCH_CHROME PORTABLE_QUICK_ACTIONS
    PORTABLE_QUICK_RADIOS ALARM_SERVICE_TAGGED_V2 PORTABLE_X4_IDLE_POLICY PORTABLE_LOW_BATTERY
    PORTABLE_ALARM_BUSY_DEFER PORTABLE_RASTER_SNAPSHOT'''.split()
    rows = []
    roots = [('fixed', ROOT)]
    if a.baseline_system:
        roots.insert(0, ('original', a.baseline_system.resolve()))
    for label, root in roots:
        for terminal in (False, True):
            for san in (False, True):
                binary = out / f'{label}-terminal{int(terminal)}-san{int(san)}'
                flags = ['-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror']
                flags += ['-D' + f for f in defines]
                if terminal:
                    flags += ['-DPORTABLE_ALARM_TERMINAL_RETENTION']
                if san:
                    flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie']
                sources = [root / 'test/native_apps/sparse_clock_busy_progress_test.c']
                sources += [root / 'lib/PortableApps/src' / n for n in (
                    'quick_actions.c', 'quick_session.c', 'quick_render.c', 'quick_radios.c')]
                subprocess.run([os.environ.get('CC', 'cc'), *flags, '-I' + str(inc),
                    '-I' + str(root / 'lib/NativeApps/include'), *map(str, sources), '-o', str(binary)], check=True)
                for case in ('busy-cold', 'busy-get', 'busy-put', 'context', 'unknown', 'alarm-frame'):
                    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1')
                    env.pop('EXPECT_ORIGINAL_FAILURE', None)
                    if label == 'original': env['EXPECT_ORIGINAL_FAILURE'] = '1'
                    result = subprocess.run([str(binary), case], capture_output=True, text=True, env=env, timeout=20)
                    (out / f'{binary.name}-{case}.log').write_text(result.stdout + result.stderr)
                    print(label, terminal, san, case, result.stdout.strip(), result.stderr.strip(), flush=True)
                    result.check_returncode()
                    rows.append(dict(source=label, terminal=terminal, sanitized=san, case=case, result=result.stdout.strip()))
    tracked = ['lib/PortableApps/src/sparse_clock_adapter.inc', 'lib/PortableApps/src/alarm.inc',
               'lib/PortableApps/src/adapter.c', 'test/native_apps/sparse_clock_busy_progress_test.c',
               'test/native_apps/sparse_clock_adapter_test.c', 'scripts/test_sparse_clock_busy_progress.py']
    receipt = dict(cases=rows, process_cases=len(rows), hardware_tested=False,
        boundary='Real sparse adapter, checked KV facade, alarm scheduling, raster replay and frame completion; modeled Runtime/device callbacks.',
        source_sha256={s: hashlib.sha256((ROOT / s).read_bytes()).hexdigest() for s in tracked})
    (out / 'evidence.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(f'{len(rows)} sparse cold-start/BUSY/frame/terminal regression processes passed')

if __name__ == '__main__': main()

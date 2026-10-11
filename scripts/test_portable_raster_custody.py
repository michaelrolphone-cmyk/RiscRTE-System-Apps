#!/usr/bin/env python3
"""Run additive raster custody gates without weakening existing fixture checks."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--sanitize', action='store_true')
parser.add_argument('--text-scene', action='store_true')
variant = parser.add_mutually_exclusive_group()
variant.add_argument('--native-terminal', action='store_true')
variant.add_argument('--clipped-begin', action='store_true')
args = parser.parse_args()
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
command = [os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra',
           '-Werror', '-Wno-unused-function', '-DPORTABLE_RASTER_SNAPSHOT',
           '-I' + str(ROOT / 'lib/PortableApps/include'),
           '-I' + str(ROOT / 'lib/NativeApps/include')]
if args.clipped_begin:
    command += ['-DRASTER_CUSTODY_CLIP', '-DPORTABLE_TOUCH_SCROLL',
                '-DPORTABLE_RETAINED_RGB565_HANDOFF', '-DPORTABLE_FORCE_FULL_FRAMES']
else:
    command.append('-DPORTABLE_NOVA_UI')
if args.text_scene:
    command.append('-DPORTABLE_TEXT_INPUT_CLIENT')
if args.native_terminal:
    command += ['-DPORTABLE_NATIVE_CUSTODY_FENCE', '-DPORTABLE_ALARM_TERMINAL_RETENTION',
                '-Wl,--wrap=free']
if args.sanitize:
    command += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                '-fno-omit-frame-pointer']
    if sys.platform != 'darwin':
        command.append('-no-pie')
command.append(str(ROOT / 'test/native_apps/portable_raster_custody_test.c'))
if not args.clipped_begin:
    command.append(str(ROOT / 'Apps/settings.c'))
    command += [str(ROOT / 'lib/PortableApps/src' / name)
                for name in ('quick_actions.c', 'quick_render.c', 'quick_session.c')]
command += ['-o', str(out / 'test')]
(out / 'compile-command.json').write_text(json.dumps(command, indent=2) + '\n')
subprocess.run(command, check=True, timeout=120)
cases = ('no-op-settle', 'failed-cleanup', 'non-cooperative-clear',
         'sticky-direct-restore', 'quick-direct-overlay', 'quick-interrupted',
         'quick-interrupted-partial', 'quick-action-partial', 'quick-real-open-close',
         'bitmap-allocation-acquire-failure', 'aborted-recording-cleanup',
         'failed-partial-cleanup', 'offscreen-allocation-recovery', 'quick-watch-immutable',
         'quick-watch-state-oom', 'quick-watch-node-oom', 'quick-watch-capacity',
         'quick-watch-offscreen-oom')
if args.text_scene:
    cases = ('text-scene-pending', 'text-scene-unsubmitted')
if args.native_terminal:
    cases = ('terminal-band-restore', 'terminal-watch-row', 'terminal-offscreen-fallback',
             'terminal-quick-orphan', 'terminal-bitmap-orphan', 'terminal-recording-callbacks',
             'terminal-repeated-quick-orphan')
if args.clipped_begin:
    cases = ('clipped-begin-equivalence',)
failures = []
for case in cases:
    result = subprocess.run([str(out / 'test'), case], text=True, capture_output=True,
                            timeout=30, env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0',
                                             'UBSAN_OPTIONS': 'halt_on_error=1'})
    (out / (case + '.log')).write_text(result.stdout + result.stderr)
    print(result.stdout + result.stderr, end='')
    if result.returncode:
        failures.append({'case': case, 'returncode': result.returncode})
(out / 'result.json').write_text(json.dumps({'cases': len(cases), 'failures': failures}, indent=2) + '\n')
if failures:
    raise SystemExit('Raster custody failures: ' + ', '.join(item['case'] for item in failures))
print(f'Raster custody: {len(cases)} cases passed')

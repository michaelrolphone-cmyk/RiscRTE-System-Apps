#!/usr/bin/env python3
"""Qualify setup cadence and cleanup fencing in the production Files adapter."""
import argparse
import json
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--sdk-include', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--sanitize', action='store_true')
parser.add_argument('--stage-logs', action='store_true')
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
cases = ['acquire-empty', 'acquire-dirty', 'acquire-lost', 'acquire-wrong-instance', 'support', 'input', 'raster', 'snapshot', 'display', 'async-display',
         'cleanup', 'cleanup-fini', 'cleanup-terminal', 'battery-refusal', 'sleep-refusal', 'recursive']
cases += ['rgb-' + case for case in ['input', 'raster', 'snapshot', 'display', 'async-display',
                                     'cleanup', 'cleanup-fini', 'cleanup-terminal', 'battery-refusal',
                                     'sleep-refusal', 'recursive']]
flags = ['PORTABLE_NATIVE_TIME_TOOLBAR', 'PORTABLE_NATIVE_CUSTODY_FENCE',
         'ALARM_SERVICE_TAGGED_V2', 'TEST_NATIVE_TOOLBAR_QUICK', 'PORTABLE_QUICK_ACTIONS',
         'PORTABLE_QUICK_RADIOS', 'PORTABLE_ALARM_CLIENT', 'PORTABLE_INPUT_NAVIGATION',
         'PORTABLE_WIFI_INSTANCE=15u', 'PORTABLE_LOW_BATTERY', 'PORTABLE_BLE_BROADCAST',
         'PORTABLE_BLE_BROADCAST_DEFAULT_OFF', 'PORTABLE_HOME_APP="default.elf"',
         'PORTABLE_RETURN_APP="parent.elf"', 'TEST_FILE_SHARING_ADAPTER',
         'PORTABLE_FILE_BROWSER_APP', 'PORTABLE_FILE_SHARING', 'PORTABLE_FILE_SETUP',
         'TEST_FILE_SETUP_ADAPTER', 'PORTABLE_FILE_SHARING_SETUP_INSTANCE=31u', 'PORTABLE_APP_SLEEP_LOCAL', 'PORTABLE_CROWN_SLEEP_LOCAL',
         'PORTABLE_ALARM_TERMINAL_RETENTION', 'PORTABLE_RASTER_SNAPSHOT']
if args.stage_logs:
    flags.append('PORTABLE_STAGE_LOGS')
helpers = ['PortableNativeTimeSource.c', 'PortableRealtimeClient.c', 'PortableTimeZone.c',
           'PortableTimeZoneCatalog.c', 'PortableTimeZonePreference.c', 'quick_actions.c',
           'quick_render.c', 'quick_session.c', 'quick_radios.c']
with tempfile.TemporaryDirectory(prefix='file-setup-adapter-') as temporary:
    include = Path(temporary) / 'include'
    shutil.copytree(ROOT / 'lib/PortableApps/include', include)
    shutil.copytree(ROOT / 'lib/PortableApps/time', Path(temporary) / 'time')
    for header in ['AlarmServiceV1.h', 'AlarmServiceV2.h', 'RiscRealtimeV1.h']:
        shutil.copyfile(args.sdk_include / header, include / header)
    binary = args.output.resolve() / 'file-setup-adapter'
    command = [os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra',
               '-Werror', '-Wno-unused-function', '-Wno-misleading-indentation',
               *['-D' + flag for flag in flags], '-I' + str(include),
               '-I' + str(ROOT / 'lib/NativeApps/include'),
               str(ROOT / 'test/native_apps/file_setup_adapter_test.c'),
               *[str(ROOT / 'lib/PortableApps/src' / name) for name in helpers],
               '-Wl,--wrap=free', '-o', str(binary)]
    if args.sanitize:
        command[1:1] = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                        '-fno-omit-frame-pointer', '-no-pie']
    subprocess.run(command, check=True)
    for case in cases:
        result = subprocess.run([str(binary), case], text=True, capture_output=True,
                                timeout=30, env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0',
                                UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
        (args.output / (case + '.log')).write_text(result.stdout + result.stderr)
        print(result.stdout + result.stderr, end='', flush=True)
        result.check_returncode()
(args.output / 'evidence.json').write_text(json.dumps({
    'cases': cases, 'sanitized': args.sanitize, 'stage_logs': args.stage_logs, 'compile': command,
    'source_sha256': {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                     for path in [ROOT / 'lib/PortableApps/src' / name for name in
                                  ['adapter.c', 'native_custody_adapter.inc', 'raster_snapshot_replay.inc']]
                     + [ROOT / 'test/native_apps' / name for name in
                        ['file_setup_adapter_test.c', 'wifi_adapter_deferral_test.c']]
                     + [Path(__file__).resolve()]},
    'boundary': 'Actual shared adapter; deterministic setup, provider and scheduler doubles. '
                'Not target timing, RF coexistence, or native scheduler qualification.',
    'hardware': 'not run', 'publication': 'none'}, indent=2) + '\n')

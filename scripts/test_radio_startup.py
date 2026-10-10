#!/usr/bin/env python3
"""Real native Wi-Fi controller and radio policy regressions; no radio/device I/O."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import portable_native_toolbar_build as native

ROOT = Path(__file__).resolve().parents[1]
CASES = ['radio-off', 'radio-airplane', 'radio-corrupt', 'radio-io', 'radio-missing',
         'home', 'home-refused', 'quick', 'fini-owned', 'fini-refused']

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--utilities', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, default=ROOT/'build/radio-startup')
    args = parser.parse_args(); out = args.output_dir.resolve(); out.mkdir(parents=True, exist_ok=True)
    include = out/'include'
    shutil.copytree(ROOT/'lib/PortableApps/include', include, dirs_exist_ok=True)
    shutil.copytree(ROOT/'lib/PortableApps/time', out/'time', dirs_exist_ok=True)
    for name in native.SDK_HEADERS:
        (include/name).write_bytes(subprocess.check_output(['git', '-C', args.runtime, 'show',
            native.RUNTIME_COMMIT+':sdk/app/'+name]))
    for name in native.portable_alarm_build.HEADERS:
        (include/name).write_bytes(subprocess.check_output(['git', '-C', args.utilities, 'show',
            native.portable_alarm_build.UTILITIES_COMMIT+':lib/Alarm/include/'+name]))
    flags = ['-DPORTABLE_NATIVE_TIME_TOOLBAR', '-DPORTABLE_NATIVE_CUSTODY_FENCE', '-DALARM_SERVICE_TAGGED_V2',
             '-DTEST_NATIVE_TOOLBAR_QUICK', '-DPORTABLE_QUICK_ACTIONS', '-DPORTABLE_ALARM_CLIENT',
             '-DPORTABLE_INPUT_NAVIGATION', '-DPORTABLE_HOME_APP="default.elf"',
             '-DPORTABLE_WIFI_SETTINGS_APP', '-DPORTABLE_WIFI_INSTANCE=15u', '-DPORTABLE_WIFI_STORAGE_INSTANCE=6',
             '-DWIFI_RETURN_APP="springboard.elf"']
    # Deliberately no PORTABLE_QUICK_RADIOS: this matches the deployed X4 Wi-Fi
    # profile, which must obey radio policy without granting a Bluetooth toggle.
    sources = [ROOT/'test/native_apps'/name for name in ('native_system_apps_test.c', 'native_system_app_entry.c')]
    sources += [ROOT/p for p in native.SOURCES]
    sources += [ROOT/'lib/PortableApps/src'/name for name in ('quick_actions.c', 'quick_render.c', 'quick_session.c')]
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    evidence = {'runtime_sdk': native.RUNTIME_COMMIT, 'alarm_sdk': native.portable_alarm_build.UTILITIES_COMMIT,
                'hardware': 'not run', 'cases': {}}
    for sanitized in (False, True):
        for stage in (False, True):
            label = ('asan-ubsan' if sanitized else 'normal')+('-stage' if stage else '-plain')
            san = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie'] if sanitized else []
            log = ['-DPORTABLE_STAGE_LOGS'] if stage else []
            common = [os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                      '-Wno-unused-function', *san, *log, '-I'+str(include), '-I'+str(ROOT/'lib/NativeApps/include')]
            binary = out/('wifi-'+label)
            subprocess.run([*common, *flags, *map(str, sources), '-Wl,--wrap=free', '-o', str(binary)], check=True)
            results = []
            for case in CASES:
                result = subprocess.run([binary, case], check=True, capture_output=True, text=True, env=env)
                results.append(result.stdout)
            quick = out/('quick-'+label)
            subprocess.run([*common, str(ROOT/'test/native_apps/quick_radio_test.c'),
                str(ROOT/'lib/PortableApps/src/quick_radios.c'), str(ROOT/'lib/PortableApps/src/quick_actions.c'), '-o', str(quick)], check=True)
            results.append(subprocess.run([quick], check=True, capture_output=True, text=True, env=env).stdout)
            (out/(label+'.log')).write_text(''.join(results)); evidence['cases'][label] = CASES+['quick-policy']
            print(label+': native Wi-Fi policy and Quick radio startup/lifecycle passed', flush=True)
    # Exercise connection, scan, cancellation, failed cleanup and app/sleep
    # handoff with the real portable controller and its secret-rejecting sink.
    binary = out/'portable-wifi-stage'
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
        '-DPORTABLE_STAGE_LOGS', '-I'+str(ROOT/'lib/PortableApps/include'), '-I'+str(ROOT/'lib/NativeApps/include'),
        str(ROOT/'test/native_apps/portable_wifi_test.c'), '-o', str(binary)], check=True)
    with (out/'portable-wifi-stage.log').open('w') as log:
        for case in range(48):
            subprocess.run([binary, str(case)], check=True, stdout=log, env=env)
    evidence['cases']['portable-wifi-stage'] = list(range(48))
    (out/'evidence.json').write_text(json.dumps(evidence, indent=2)+'\n')

if __name__ == '__main__': main()

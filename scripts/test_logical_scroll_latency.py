#!/usr/bin/env python3
"""Production scroll controllers under fast, 60 Hz and slow paper completion.
Input uses a sequenced raw queue. Only provider doubles are simulated; no
hardware, radio, firmware flashing, or publication occurs.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
HELPERS = ['PortableRealtimeClient', 'PortableTimeZone', 'PortableTimeZoneCatalog', 'PortableTimeZonePreference']
COMMON = 'PORTABLE_TOUCH_SCROLL PORTABLE_PAPER_TRANSITIONS PORTABLE_NATIVE_CUSTODY_FENCE'.split()
NATIVE = 'PORTABLE_NATIVE_TIME_TOOLBAR ALARM_SERVICE_TAGGED_V2 TEST_NATIVE_TOOLBAR_QUICK PORTABLE_QUICK_ACTIONS PORTABLE_ALARM_CLIENT PORTABLE_INPUT_NAVIGATION'.split()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', type=Path, required=True, help='Verified Runtime/Realtime and tagged Alarm SDK directory')
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--normal-only', action='store_true')
    parser.add_argument('--snapshot', action='store_true', help='Use the selected immutable raster pipeline')
    parser.add_argument('--controller', action='append', choices=['settings', 'wifi', 'files', 'springboard', 'springboard-legacy'])
    parser.add_argument('--case', action='append', choices=['root', 'fields', 'timezone', 'touch', 'confirm', 'drag', 'changed'])
    args = parser.parse_args()
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    include = output / 'sdk/include'
    shutil.copytree(ROOT / 'lib/PortableApps/include', include, dirs_exist_ok=True)
    shutil.copytree(ROOT / 'lib/PortableApps/time', include.parent / 'time', dirs_exist_ok=True)
    for name in ['RiscRuntimeV1.h', 'RiscRealtimeV1.h', 'AlarmServiceV1.h', 'AlarmServiceV2.h']:
        shutil.copyfile(args.sdk / name, include / name)
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    results = []
    for sanitized in ([False] if args.normal_only else [False, True]):
        sanitizer = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie'] if sanitized else []
        for controller in args.controller or ['settings', 'wifi', 'files', 'springboard', 'springboard-legacy']:
            for short in ([False, True] if controller in ('settings', 'wifi', 'files') else [False]):
                flags = COMMON.copy()
                if args.snapshot:flags += ['PORTABLE_RASTER_SNAPSHOT']
                if controller == 'settings':
                    flags += ['PORTABLE_SETTINGS_LIST_SCROLL']
                    if short:
                        flags += ['TEST_NATIVE_SETTINGS_SHORT']
                    sources = ['Apps/settings_native_entry.c', 'test/native_apps/logical_scroll_settings_latency_test.c']
                    helpers = ['PortableSetTime', *HELPERS]
                    cases = ['root', 'fields', 'timezone', 'drag']
                else:
                    flags += [*NATIVE, 'PORTABLE_HOME_APP="default.elf"']
                    helpers = ['PortableNativeTimeSource', *HELPERS, 'quick_actions', 'quick_render', 'quick_session']
                    if short:
                        flags += ['TEST_TOOLBAR_NATIVE_WIDTH=600', 'TEST_TOOLBAR_NATIVE_HEIGHT=400']
                    cases = ['touch', 'confirm', 'drag']
                    if controller == 'wifi':
                        flags += ['PORTABLE_WIFI_SETTINGS_APP', 'PORTABLE_WIFI_INSTANCE=15u', 'PORTABLE_WIFI_STORAGE_INSTANCE=6', 'WIFI_RETURN_APP="springboard.elf"']
                    elif controller == 'files':
                        cases += ['changed']
                        flags += ['PORTABLE_FILE_BROWSER_APP', 'PORTABLE_NOVA_UI', 'PORTABLE_APP_OWNS_TOUCH_CHROME', 'PORTABLE_FILE_BROWSER_CAPABILITY="storage.volume"', 'PORTABLE_FILE_BROWSER_INSTANCE=9u', 'PORTABLE_FILE_BROWSER_HANDLERS', 'FILE_BROWSER_RETURN_APP="springboard.elf"']
                    else:
                        flags += ['PORTABLE_APP_TOUCH_SCROLL', 'PORTABLE_NOVA_UI', 'PORTABLE_SPRINGBOARD_CATALOG_BOUND=22']
                        if controller == 'springboard':
                            flags += ['PORTABLE_SPRINGBOARD_TOUCH_SCROLL']
                        else:
                            cases = ['touch', 'confirm']
                    if controller.startswith('springboard'):
                        sources = ['test/native_apps/logical_scroll_springboard_entry.c', 'test/native_apps/logical_scroll_springboard_latency_test.c']
                    else:
                        sources = [f'test/native_apps/touch_scroll_{controller}_entry.c', 'test/native_apps/logical_scroll_apps_latency_test.c']
                label = controller + ('-short' if short else '') + ('-san' if sanitized else '')
                binary = output / label
                command = [os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', *sanitizer, *['-D' + flag for flag in flags], '-I' + str(include), '-I' + str(ROOT / 'lib/NativeApps/include')]
                command += [str(ROOT / name) for name in sources]
                command += [str(ROOT / 'lib/PortableApps/src' / (name + '.c')) for name in helpers]
                subprocess.run([*command, '-Wl,--wrap=free', '-o', str(binary)], check=True)
                for case in [c for c in cases if not args.case or c in args.case]:
                    for latency in [0, 17, 2300]:
                        run = subprocess.run([str(binary), case, str(latency)], text=True, capture_output=True, env=env, timeout=30)
                        if run.returncode:
                            raise AssertionError((label, case, latency, run.stdout, run.stderr))
                        result = dict(profile=label, sanitized=sanitized, **json.loads(run.stdout))
                        results.append(result)
                        print(json.dumps(result), flush=True)
    sources = [*ROOT.glob('test/native_apps/logical_scroll*'), ROOT / 'test/native_apps/logical_touch_queue_fixture.h', *ROOT.glob('Apps/*scroll*.inc'), ROOT / 'Apps/springboard_paper.inc', ROOT / 'Apps/springboard_pages.h', ROOT / 'lib/PortableApps/include/PortableTouch.h', ROOT / 'lib/PortableApps/include/PortableTouchScroll.h', ROOT / 'lib/PortableApps/src/adapter.c', *ROOT.glob('lib/PortableApps/src/settings*.inc'), ROOT / 'lib/PortableApps/src/wifi_scroll.inc', Path(__file__).resolve()]
    evidence = dict(hardware='not run', sdk_sha256={name: hashlib.sha256((include / name).read_bytes()).hexdigest() for name in ['RiscRuntimeV1.h', 'RiscRealtimeV1.h', 'AlarmServiceV1.h', 'AlarmServiceV2.h']}, runs=results, process_cases=len(results), source_sha256={str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources})
    (output / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')


if __name__ == '__main__':
    main()

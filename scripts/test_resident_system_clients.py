#!/usr/bin/env python3
"""Actual Files/Wi-Fi/update/USB clients and shared host in production Runtime.

Separate dlopen ELFs use real System controllers and adapters. Controllers are
seeded at a settled screen for explicit checkpoint assertions, then their normal
Home loop runs. Providers, media and clocks are deterministic fixtures; no
hardware, USB transport, Windows hang or console restoration is qualified.
"""
import argparse
import hashlib
import json
import os
import shutil
import subprocess
from pathlib import Path
import portable_native_toolbar_build as native

ROOT = Path(__file__).resolve().parents[1]
CAPS = [('display.output', 1), ('input.touch.raw', 1), ('input.navigation', 1),
        ('board.battery', 1), ('rtc.clock', 2), ('alarm.service', 2), ('net.wifi', 1),
        ('bluetooth.hci', 1), ('storage.volume', 1), ('software.update.apps', 1),
        ('software.update.firmware', 1), ('usb.device.msc', 1)]
CASES = {
    'files': ['overlay', 'busy', 'cleanup', 'cleanup-terminal', 'terminal', 'poll', 'capture', 'file-open'],
    'wifi': ['overlay', 'busy', 'cleanup', 'cleanup-terminal', 'terminal', 'poll', 'capture', 'policy-busy'],
    'apps': ['overlay', 'busy', 'cleanup', 'cleanup-terminal', 'terminal', 'poll', 'capture', 'policy-busy'],
    'ota': ['overlay', 'busy', 'cleanup', 'cleanup-terminal', 'terminal', 'poll', 'capture', 'policy-busy'],
    'usb': ['overlay', 'busy', 'cleanup', 'terminal', 'poll', 'eject', 'release-retained'],
}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--runtime', type=Path, required=True)
    p.add_argument('--app-runtime-sdk', type=Path, help='Compile new app headers separately from the selected Runtime implementation')
    p.add_argument('--display-sdk', type=Path, required=True)
    p.add_argument('--alarm-sdk', type=Path, required=True)
    p.add_argument('--msc-sdk', type=Path, required=True)
    p.add_argument('--output-dir', type=Path, default=ROOT/'build/resident-system-clients')
    p.add_argument('--normal-only', action='store_true')
    p.add_argument('--app', action='append', choices=CASES)
    p.add_argument('--case', action='append')
    args = p.parse_args()
    out = args.output_dir.resolve()
    inc = out/'include'
    shutil.copytree(ROOT/'lib/PortableApps/include', inc, dirs_exist_ok=True)
    shutil.copytree(ROOT/'lib/PortableApps/time', out/'time', dirs_exist_ok=True)
    for name in ('RiscRuntimeV1.h', 'RiscResidentShellV1.h', 'RiscRealtimeV1.h'):
        shutil.copyfile((args.app_runtime_sdk or args.runtime/'sdk/app')/name, inc/name)
    if (args.runtime/'sdk/app/RiscFailureEvidenceV1.h').is_file():
        shutil.copyfile(args.runtime/'sdk/app/RiscFailureEvidenceV1.h',inc/'RiscFailureEvidenceV1.h')
    for name in ('RiscDisplayOutputV1.h', 'RiscDisplayOutputPowerV1.h',
                 'RiscDisplayOutputMetricsV1.h', 'RiscDisplayOutputSnapshotV1.h'):
        shutil.copyfile(args.display_sdk/name, inc/name)
    shutil.copyfile(args.msc_sdk/'RiscUsbDeviceMscV1.h', inc/'RiscUsbDeviceMscV1.h')
    for name in ('AlarmServiceV1.h', 'AlarmServiceV2.h'):
        shutil.copyfile(args.alarm_sdk/name, inc/name)
    common = ['-DPORTABLE_PAPER_PREFERENCES', '-DPORTABLE_ALARM_TERMINAL_RETENTION',
              '-DPORTABLE_NATIVE_CUSTODY_FENCE', '-DPORTABLE_ALARM_CLIENT', '-DALARM_SERVICE_TAGGED_V2',
              '-DPORTABLE_INPUT_NAVIGATION', '-DPORTABLE_APP_OWNS_TOUCH_CHROME',
              '-DPORTABLE_HOME_APP="default.elf"', '-DPORTABLE_RESIDENT_POLICY']
    client = ['-DPORTABLE_RESIDENT_SHELL_CLIENT', '-DPORTABLE_NATIVE_TIME_TOOLBAR', '-DTEST_IDLE_ELIGIBILITY']
    profiles = {
        'files': ['-DPORTABLE_FILE_BROWSER_APP', '-DPORTABLE_FILE_BROWSER_CAPABILITY="storage.volume"',
                  '-DPORTABLE_FILE_BROWSER_INSTANCE=9u', '-DPORTABLE_FILE_BROWSER_HANDLERS',
                  '-DPORTABLE_NOVA_UI', '-DPORTABLE_TOUCH_SCROLL'],
        'wifi': ['-DPORTABLE_WIFI_SETTINGS_APP', '-DPORTABLE_WIFI_INSTANCE=15u',
                 '-DPORTABLE_WIFI_STORAGE_INSTANCE=6', '-DPORTABLE_TOUCH_SCROLL'],
        'apps': ['-DPORTABLE_UPDATE_APP', '-DPORTABLE_UPDATE_FIRMWARE=0', '-DPORTABLE_WIFI_INSTANCE=15u',
                 '-DPORTABLE_TOUCH_SCROLL', '-DPORTABLE_APP_TOUCH_SCROLL', '-DPORTABLE_UPDATE_TOUCH_SCROLL'],
        'ota': ['-DPORTABLE_UPDATE_APP', '-DPORTABLE_UPDATE_FIRMWARE=1', '-DPORTABLE_WIFI_INSTANCE=15u',
                '-DPORTABLE_TOUCH_SCROLL', '-DPORTABLE_APP_TOUCH_SCROLL', '-DPORTABLE_UPDATE_TOUCH_SCROLL'],
        'usb': ['-DPORTABLE_USB_TRANSFER_APP', '-DPORTABLE_APP_LAUNCH_GUARD'],
    }
    sources = [ROOT/'test/native_apps/resident_system_app.c', ROOT/'test/native_apps/resident_system_adapter.c']
    results = []
    for sanitized in ([False] if args.normal_only else [False, True]):
        build = out/('sanitized' if sanitized else 'normal')
        build.mkdir(parents=True, exist_ok=True)
        san = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer'] if sanitized else []
        base = ['-O1', '-g', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', '-Wno-unused-variable',
                '-Wno-misleading-indentation', *san, '-I'+str(inc), '-I'+str(ROOT/'lib/NativeApps/include')]
        for name in ['host', *(args.app or CASES)]:
            flags = list(common)
            linked = list(sources)
            if name == 'host':
                flags += ['-DPORTABLE_RESIDENT_SHELL_HOST', '-DPORTABLE_APP_SLEEP_LOCAL',
                          '-DPORTABLE_QUICK_ACTIONS', '-DPORTABLE_QUICK_RADIOS',
                          '-DPORTABLE_PAPER_TRANSITIONS', '-DPORTABLE_X4_IDLE_POLICY', '-DPORTABLE_LOW_BATTERY']
                linked += [ROOT/'lib/PortableApps/src'/n for n in ('quick_actions.c', 'quick_session.c', 'quick_render.c', 'quick_radios.c')]
            else:
                flags += [f for f in client if name != "usb" or f != "-DPORTABLE_NATIVE_TIME_TOOLBAR"] + profiles[name]
                # Synthetic capture hooks exercise the adapter inhibit branch.
                if name != 'usb':
                    flags += ['-DPORTABLE_PAPER_TRANSITIONS', '-DPORTABLE_AUDIO_SESSION', '-DPORTABLE_AUDIO_CONTINUOUS_CAPTURE']
                linked += [ROOT/s for s in native.SOURCES]
            elf = build/(name+'.elf')
            subprocess.run(['cc', '-std=c11', *base, '-fPIC', '-shared', '-Wl,-Bsymbolic', *flags,
                            *map(str, linked), '-o', str(elf)], check=True)
            symbols = subprocess.check_output(['nm', str(elf)], text=True)
            assert (' pqa_render' in symbols) == (name == 'host'), name
            assert ' risc_resident_app_descriptor_v1' in symbols
        subprocess.run(['cc', '-std=c11', *base, '-fPIC', '-shared', str(ROOT/'test/native_apps/resident_system_file_receiver.c'), '-o', str(build/'receiver.elf')], check=True)
        fixture = ROOT/'test/native_apps/resident_system_test.c'
        for index, (cap, version) in enumerate(CAPS, 1):
            subprocess.run(['cc', '-std=c11', *base, '-I'+str(args.runtime/'sdk/driver'), '-fPIC', '-shared',
                            '-DPOLICY_PROVIDER='+str(index), '-DPOLICY_CAPABILITY="'+cap+'"', '-DPOLICY_API='+str(version),
                            str(fixture), '-o', str(build/('provider-'+str(index)+'.elf'))], check=True)
        subprocess.run(['cc', '-std=c11', *base, '-DPORTABLE_ALARM_CLIENT', '-DALARM_SERVICE_TAGGED_V2',
                        '-DPORTABLE_X4_IDLE_POLICY', '-c', str(fixture), '-o', str(build/'fixture.o')], check=True)
        rincs = ['-I'+str(args.runtime/path) for path in ('src', 'sdk/app', 'sdk/driver', 'sdk/hardware',
                 'lib/ArduinoJson/src', 'test/drivers/stubs')]
        rsources = [args.runtime/path for path in ('src/bootstrap/Json.cpp', 'src/bootstrap/Board.cpp',
                    'src/bootstrap/Runtime.cpp', 'src/runtime/streams/AppStreamSessions.cpp',
                    'src/runtime/streams/ProviderQueueHost.cpp', 'src/runtime/drivers/ProviderGraphV2.cpp',
                    'src/runtime/drivers/ProviderModuleV2.cpp')]
        binary = build/'resident-system-test'
        subprocess.run(['c++', '-std=c++17', '-g', '-Wall', '-Wextra', '-Werror', '-Wno-missing-field-initializers',
                        *san, *rincs, '-rdynamic', *(['-no-pie'] if sanitized else []), *map(str, rsources),
                        str(ROOT/'test/native_apps/resident_system_runtime.cpp'), str(build/'fixture.o'), '-ldl', '-o', str(binary)], check=True)
        for app_name in args.app or CASES:
            shutil.copyfile(build/(app_name+'.elf'), build/'client.elf')
            for case in args.case or CASES[app_name]:
                result = subprocess.run([str(binary), str(build), case], check=True, timeout=30,
                                        text=True, stdout=subprocess.PIPE, env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
                print(app_name, 'sanitized' if sanitized else 'normal', result.stdout.strip(), flush=True)
                results.append({'app': app_name, 'case': case, 'sanitized': sanitized, 'result': result.stdout.strip()})
    evidence = {'runtime_commit': subprocess.check_output(['git', '-C', str(args.runtime), 'rev-parse', 'HEAD'], text=True).strip(),
                'hardware_tested': False, 'runtime_mocked': False,
                'sdk_sha256': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in inc.glob('*.h')},
                'system_commit': subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip(),
                'boundary': 'Production Runtime/Graph and separate host/client/provider ELFs; seeded actual controller models, explicit adapter checkpoints, normal controller Home loops, synthetic provider callbacks.',
                'runs': results, 'process_cases': len(results),
                'source_sha256': {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                                  for p in [Path(__file__).resolve(), *ROOT.glob('test/native_apps/resident_system_*'),
                                            *ROOT.glob('Apps/file_browser*'), *ROOT.glob('Apps/wifi_settings*'),
                                            *ROOT.glob('Apps/update_*.inc'), ROOT/'Apps/usb_sd_transfer.c',
                                            *ROOT.glob('lib/PortableApps/src/*') ] if p.is_file()}}
    (out/'evidence.json').write_text(json.dumps(evidence, indent=2)+'\n')


if __name__ == '__main__':
    main()

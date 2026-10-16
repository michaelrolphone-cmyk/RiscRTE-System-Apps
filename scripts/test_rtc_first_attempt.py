#!/usr/bin/env python3
"""Compose real Settings save/controller with both production RTC providers.

I2C, host UI, storage, monotonic clock and output dependencies are modeled.
The alarm profile uses the production Points/volume/DND clock-change service.
This does not qualify physical oscillator startup or an unidentified old ELF.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--waveshare', type=Path, required=True)
    parser.add_argument('--twatch', type=Path, required=True)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--utilities', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/rtc-first-attempt')
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    watch, wave = args.twatch.resolve(), args.waveshare.resolve()
    utilities = args.utilities.resolve()
    sources = {
        'provider_fixture': ROOT / 'test/native_apps/rtc_first_attempt_provider.c',
        'alarm_fixture': ROOT / 'test/native_apps/rtc_first_attempt_alarm.c',
        'settings_fixture': ROOT / 'test/native_apps/rtc_first_attempt_test.c',
        'settings': ROOT / 'lib/PortableApps/src/settings.inc',
        'alarm': utilities / 'Services/alarm_service/service.c',
        'controller': ROOT / 'Apps/settings.c',
        'pcf8563': watch / 'drivers/twatch_rtc/driver.c',
        'pcf85063': wave / 'drivers/rtc_pcf85063/driver.c',
    }
    evidence = {'hardware_tested': False, 'target_instructions_executed': False,
                'shipping_settings_elf_reproduced': False,
                'source_sha256': {key: hashlib.sha256(p.read_bytes()).hexdigest()
                                  for key, p in sources.items()}, 'runs': []}
    for name, repo in (('system', ROOT), ('runtime', args.runtime.resolve()), ('utilities', utilities), ('twatch', watch), ('waveshare', wave)):
        evidence[name + '_revision'] = subprocess.check_output(
            ['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip()
    for device in ('pcf85063', 'pcf8563'):
        includes = [wave / 'include', watch / 'sdk/driver', watch / 'sdk/app', watch / 'include']
        for sanitized in (False, True):
            label = device + ('-sanitized' if sanitized else '-normal')
            common = [os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror']
            if sanitized:
                common += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                           '-fno-omit-frame-pointer', '-no-pie']
            provider = output / (label + '.o')
            command = [*common, *['-I' + str(p) for p in includes],
                       '-DRTC_PRODUCTION_SOURCE="' + str(sources[device]) + '"',
                       *(['-DRTC_PCF85063'] if device == 'pcf85063' else []),
                       '-c', str(ROOT / 'test/native_apps/rtc_first_attempt_provider.c'), '-o', str(provider)]
            subprocess.run(command, check=True, timeout=60)
            alarm = output / (label + '-alarm.o')
            alarm_flags = ['-DALARM_CLOCK_CHANGE_CONTROL', '-DPOINTS_IN_TIME_SERVICE',
                           '-DALARM_VOLUME_CONTROL', '-DALARM_DND_CONTROL', '-DPORTABLE_RTC_UTC8_DENVER']
            subprocess.run([*common, *alarm_flags, *['-I' + str(p) for p in includes],
                            '-I' + str(utilities / 'lib/Alarm/include'),
                            '-I' + str(args.runtime.resolve() / 'sdk/driver'),
                            '-I' + str(args.runtime.resolve() / 'sdk/app'),
                            '-I' + str(ROOT / 'lib/PortableApps/include'),
                            '-DALARM_PRODUCTION_SOURCE="' + str(sources['alarm']) + '"',
                            '-c', str(ROOT / 'test/native_apps/rtc_first_attempt_alarm.c'),
                            '-o', str(alarm)], check=True, timeout=60)
            for zone in ('raw', 'denver'):
                for path in ('ui', 'alarm'):
                    binary = output / (label + '-' + zone + '-' + path)
                    flags = ['-DPORTABLE_RTC_UTC8_DENVER'] if zone == 'denver' else []
                    if path == 'alarm':
                        flags += ['-DTEST_FIRST_ALARM']
                    subprocess.run([*common, *flags, '-I' + str(ROOT / 'lib/PortableApps/include'),
                                    '-I' + str(ROOT / 'lib/NativeApps/include'),
                                    str(ROOT / 'Apps/settings.c'),
                                    str(ROOT / 'test/native_apps/rtc_first_attempt_test.c'), str(provider),
                                    *([str(alarm)] if path == 'alarm' else []),
                                    '-o', str(binary)], check=True, timeout=120)
                    cases = ('ui',) if path == 'ui' else ('ui', 'save', 'retry-read', 'fail-read', 'fail-write', 'fail-alarm-read', 'unannounced')
                    for case in cases:
                        for invalid in range(4):
                            run = subprocess.run([str(binary), case, str(invalid)],
                                                 text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                                 env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0'), timeout=20)
                            if run.returncode:
                                print(run.stdout, flush=True)
                                run.check_returncode()
                            evidence['runs'].append({'profile': label + '-' + zone + '-' + path, 'case': case,
                                                     'initial': invalid, 'result': run.stdout.strip()})
                    print(binary.name + ': first-save/checked-fault cases passed', flush=True)
    (output / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print('256 composed first-save cases passed; hardware and old binary closure unverified.')


if __name__ == '__main__':
    main()

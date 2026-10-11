#!/usr/bin/env python3
"""Exercise Watch's checked Settings cleanup and local-sleep idle helpers."""
import argparse
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--runtime', required=True, type=Path)
args = parser.parse_args()
out = ROOT / 'build/watch-checked-settings'
out.mkdir(parents=True, exist_ok=True)
for sanitized in (False, True):
    flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
             '-fno-omit-frame-pointer', '-no-pie'] if sanitized else []
    common = [
        os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
        *flags, '-I' + str(ROOT / 'lib/PortableApps/include'),
        '-I' + str(ROOT / 'lib/NativeApps/include'),
        '-I' + str(args.runtime.resolve() / 'sdk/app'),
    ]
    suffix = '-san' if sanitized else ''
    for alarm in (False, True):
        binary = out / ('cleanup' + ('-alarm' if alarm else '') + suffix)
        subprocess.run([
            *common, *(['-DPORTABLE_ALARM_SETTINGS'] if alarm else []),
            str(ROOT / 'Apps/settings.c'),
            str(ROOT / 'test/native_apps/watch_checked_settings_cleanup_test.c'),
            '-o', str(binary),
        ], check=True, timeout=60)
        for scenario in range(3):
            subprocess.run([str(binary), str(scenario)], check=True, timeout=20)
    # These fixtures select local sleep themselves. Do not add the broader
    # TEST_IDLE_ELIGIBILITY define: that would hide the Watch linker regression.
    for name, fixture, scenario in (
            ('wifi-idle', 'portable_wifi_test.c', 61),
            ('update-idle', 'portable_update_app_test.c', 40)):
        binary = out / (name + suffix)
        subprocess.run([
            *common, '-Wno-misleading-indentation',
            str(ROOT / 'test/native_apps' / fixture), '-o', str(binary),
        ], check=True, timeout=60)
        subprocess.run([str(binary), str(scenario)], check=True, timeout=20)
print('Watch profile: 12 checked Settings and 4 local-sleep idle cases passed in normal/ASan/UBSan modes')

#!/usr/bin/env python3
"""Default-off namespace-6 availability controller and draft-grant fences."""
import argparse
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--sanitize', action='store_true')
parser.add_argument('--paper', action='store_true')
parser.add_argument('--output', type=Path, default=Path('/tmp/system-wifi-availability-tests'))
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
args.output.mkdir(parents=True, exist_ok=True)
flags = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g', '-no-pie'] if args.sanitize else []
if args.paper:
    flags += ['-DTEST_WIFI_PAPER', '-DPORTABLE_DISPLAY_ROTATION=90']
binary = args.output/'wifi_availability'
subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-DTEST_IDLE_ELIGIBILITY', '-Wall', '-Wextra', '-Werror',
    *flags, *['-I'+str(root/path) for path in ['lib/PortableApps/include', 'lib/NativeApps/include', 'lib/RemoteFiles/vendor/tinydtls_sha2']],
    str(root/'test/native_apps/wifi_availability_test.c'), str(root/'lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c'),
    '-o', str(binary)], check=True)
for case in range(17):
    subprocess.run([str(binary), str(case)], check=True)
print('Wi-Fi default-off availability, no fallback, temporary drafts and clean reload fences passed.')

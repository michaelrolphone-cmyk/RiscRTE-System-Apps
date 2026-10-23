#!/usr/bin/env python3
"""Compile the real remote battery session against a fake capability, without RF."""
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/remote-battery'
OUT.mkdir(parents=True, exist_ok=True)
for sanitized in (False, True):
    flags = ['-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
             '-I' + str(ROOT / 'lib/PortableApps/include')]
    if sanitized:
        flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                  '-fno-omit-frame-pointer', '-no-pie']
    binary = OUT / ('session-sanitized' if sanitized else 'session')
    sources = [ROOT / 'test/native_apps/remote_battery_test.c',
               ROOT / 'lib/PortableApps/src/remote_battery.c']
    subprocess.run([os.environ.get('CC', 'cc'), *flags, *map(str, sources),
                    '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0'))
print('Remote battery session: normal and ASan/UBSan passed')

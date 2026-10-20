#!/usr/bin/env python3
"""Qualify the private Timecard owner14 visibility and input reducer."""
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
out = root / 'build/timecard-native-input'
out.mkdir(parents=True, exist_ok=True)
common = [os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
    *('-I' + str(root / p) for p in ('lib/PortableApps/include', 'lib/PortableApps/src', 'lib/NativeApps/include'))]
for sanitized in (False, True):
    flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie'] if sanitized else []
    label = 'reducer-' + ('sanitized' if sanitized else 'normal')
    binary = out / label
    subprocess.run([*common, *flags, str(root / 'test/native_apps/timecard_native_input_test.c'), '-o', str(binary)], check=True)
    result = subprocess.run([str(binary)], text=True, capture_output=True, timeout=30,
        env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0', 'UBSAN_OPTIONS': 'halt_on_error=1'})
    (out / (label + '.log')).write_text(result.stdout + result.stderr)
    print(label + ': ' + result.stdout + result.stderr, end='', flush=True)
    result.check_returncode()
print('PASS: two normal/sanitized Timecard owner14 reducer executions')

#!/usr/bin/env python3
"""Offline optional firmware policy discovery and prefix compatibility tests."""
import os
from pathlib import Path
import subprocess
import sys
ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/portable/firmware-readiness-tests'
OUT.mkdir(parents=True, exist_ok=True)
for sanitized in (False, True):
    flags = (['-fsanitize=address,undefined', '-fno-omit-frame-pointer'] +
             (['-no-pie'] if sys.platform != 'darwin' else [])) if sanitized else []
    for policy in ('watch', 'empty', 'configured'):
        defines = [] if policy == 'watch' else ['-DUPDATE_PRODUCT_X4']
        if policy == 'configured':
            defines.append('-DUPDATE_CATALOG_URL="https://fixture.invalid/updates"')
        binary = OUT / f'{policy}-{sanitized}'
        subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++17', '-Wall', '-Wextra',
                        '-Werror', '-Wno-misleading-indentation', '-DUPDATE_FIRMWARE=1',
                        *flags, *defines, '-I'+str(ROOT/'lib/PortableApps/include'),
                        '-I'+str(ROOT/'lib/NativeApps/include'),
                        str(ROOT/'test/native_apps/firmware_update_readiness_test.cpp'),
                        '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True, timeout=15)
print('Firmware readiness: three policies, legacy/truncated tables, no I/O or mutation; normal + ASan/UBSan passed')

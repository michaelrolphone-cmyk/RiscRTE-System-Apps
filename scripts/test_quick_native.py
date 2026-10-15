#!/usr/bin/env python3
"""Exercise native Quick input/raster geometry and preserve the legacy canvas."""
from pathlib import Path
import os
import subprocess
import sys
ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/quick-native'
OUT.mkdir(parents=True, exist_ok=True)
for native_height in (False, True):
    for sanitizer in (False, True):
        target = OUT / f'quick-native-{int(native_height)}-{int(sanitizer)}'
        flags = ['-DPORTABLE_WATCH_NATIVE_HEIGHT'] if native_height else []
        if sanitizer:
            flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
            if sys.platform != 'darwin':
                flags += ['-no-pie']
        command = [os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', '-pedantic',
                   '-DPORTABLE_RASTER_SNAPSHOT', *flags, '-I' + str(ROOT / 'lib/PortableApps/include'),
                   str(ROOT / 'lib/PortableApps/src/quick_actions.c'), str(ROOT / 'lib/PortableApps/src/quick_render.c'),
                   str(ROOT / 'test/native_apps/quick_native_test.c'), '-o', str(target)]
        subprocess.run(command, check=True)
        subprocess.run([str(target), str(OUT)], check=True, env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'}, timeout=180)
print('Native Quick: plain/native-height builds, normal and ASan/UBSan passed')

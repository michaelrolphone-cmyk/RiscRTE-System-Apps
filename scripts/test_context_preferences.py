#!/usr/bin/env python3
"""Exercise the actual copied-record/persistent-preference boundary."""
import os
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[1]
out = root / 'build/contexts'
out.mkdir(parents=True, exist_ok=True)
for mode, flags in [('normal', []), ('sanitized', ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-no-pie'])]:
    target = out / ('preferences-' + mode)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                    *flags, '-I' + str(root / 'lib/PortableApps/include'),
                    str(root / 'test/native_apps/context_preferences_test.c'), '-o', str(target)], check=True)
    subprocess.run([str(target)], check=True)

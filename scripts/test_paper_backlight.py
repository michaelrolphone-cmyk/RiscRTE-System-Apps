#!/usr/bin/env python3
"""Production paper gesture/session backlight toggle, normal and ASan/UBSan."""
import os
import subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/paper-backlight';OUT.mkdir(parents=True,exist_ok=True)
for sanitized in (False,True):
 binary=OUT/('session-san' if sanitized else 'session')
 flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,
                 '-DPORTABLE_PAPER_TRANSITIONS','-I'+str(ROOT/'lib/PortableApps/include'),
                 ROOT/'test/native_apps/paper_backlight_test.c',
                 ROOT/'lib/PortableApps/src/quick_actions.c',ROOT/'lib/PortableApps/src/quick_session.c',
                 '-o',binary],check=True)
 subprocess.run([binary],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'))

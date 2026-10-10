#!/usr/bin/env python3
"""Compile the production host controller for optional tone interaction cases."""
import os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/shared-quick-tone-input';OUT.mkdir(parents=True,exist_ok=True)
for sanitize in (False,True):
 target=OUT/('sanitized' if sanitize else 'normal')
 flags=['-DPORTABLE_RESIDENT_SHELL_HOST','-DPORTABLE_PAPER_TRANSITIONS','-DPORTABLE_QUICK_USB_TRANSFER','-DPORTABLE_FRONTLIGHT_TONE']
 if sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-pedantic','-g',*flags,'-I'+str(ROOT/'lib/PortableApps/include'),str(ROOT/'lib/PortableApps/src/quick_actions.c'),str(ROOT/'test/native_apps/shared_quick_tone_input_test.c'),'-o',str(target)],check=True)
 subprocess.run([target],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'))

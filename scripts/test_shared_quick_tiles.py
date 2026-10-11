#!/usr/bin/env python3
"""Host tile geometry and input with/without USB and audio capabilities."""
import os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/shared-quick-tiles';OUT.mkdir(parents=True,exist_ok=True)
for usb in (False,True):
 for sanitize in (False,True):
  target=OUT/f'tiles-{int(usb)}-{int(sanitize)}'
  flags=['-DPORTABLE_RESIDENT_SHELL_HOST','-DPORTABLE_PAPER_TRANSITIONS']
  if usb:flags+=['-DPORTABLE_QUICK_USB_TRANSFER']
  if sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
  subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-pedantic','-g',*flags,'-I'+str(ROOT/'lib/PortableApps/include'),str(ROOT/'lib/PortableApps/src/quick_actions.c'),str(ROOT/'test/native_apps/shared_quick_tiles_test.c'),'-o',str(target)],check=True)
  subprocess.run([target],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'))
print('4 normal/sanitized USB-selection host tile configurations passed')

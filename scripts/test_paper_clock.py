#!/usr/bin/env python3
"""Actual paper clock and adapter on native X4 mono geometry, no hardware."""
import os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
for san in (False,True):
 out=ROOT/'build/paper-clock'/str(int(san));out.mkdir(parents=True,exist_ok=True)
 flags=['-DTEST_NATIVE_LANDSCAPE','-DPORTABLE_DISPLAY_ROTATION=90','-DPORTABLE_APP_OWNS_TOUCH_CHROME','-DPORTABLE_RTC_WALL_TIME','-DPORTABLE_ALARM_CLIENT']
 if san:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 exe=out/'clock'
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',*flags,'-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),str(ROOT/'Apps/paper_clock.c'),str(ROOT/'lib/PortableApps/src/adapter.c'),str(ROOT/'test/native_apps/paper_clock_test.c'),'-o',str(exe)],check=True)
 for scene in range(14):
  env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',PAPER_FRAME=str(out/f'clock-{scene}.pbm'))
  subprocess.run([str(exe),str(scene)],env=env,check=True,timeout=15)
print('Clock: 28 normal/sanitized native mono cases pass; all-direction swipe, neutral gating, cancel, retry, wall time and minute-only refresh')

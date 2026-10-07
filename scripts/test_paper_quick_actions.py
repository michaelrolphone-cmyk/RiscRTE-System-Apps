#!/usr/bin/env python3
"""Real paper clock/launcher QuickActions, Home and clean-restore regressions."""
from pathlib import Path
import subprocess,os
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/paper-quick';OUT.mkdir(parents=True,exist_ok=True)
for san in (False,True):
 for landscape in (False,True):
  for launcher in (False,True):
   flags=['-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_RTC_WALL_TIME','-DPORTABLE_INPUT_NAVIGATION']
   if launcher:flags+=['-DTEST_SPRINGBOARD','-DPORTABLE_RETURN_APP="parent.elf"','-DPORTABLE_HOME_APP="default.elf"']
   else:flags+=['-DPORTABLE_APP_OWNS_TOUCH_CHROME','-DPORTABLE_CROWN_SLEEP_UNAVAILABLE']
   if landscape:flags+=['-DTEST_NATIVE_LANDSCAPE','-DPORTABLE_DISPLAY_ROTATION=90']
   if san:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
   target=OUT/f'paper-{int(landscape)}-{int(san)}-{int(launcher)}'
   sources=[ROOT/'Apps'/('springboard.c' if launcher else 'paper_clock.c'),ROOT/'test/native_apps/paper_quick_test.c']
   sources += [ROOT/'lib/PortableApps/src'/p for p in ['adapter.c','quick_actions.c','quick_render.c','quick_session.c']]
   subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),*map(str,sources),'-o',str(target)],check=True)
   for case in (range(20,28) if launcher else [*range(19),30,31,32,33]):
    frames=OUT/f'frames-{int(landscape)}-{int(san)}-{int(launcher)}-{case}';frames.mkdir(exist_ok=True)
    subprocess.run([str(target),str(case),str(frames)],check=True,timeout=20,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
print('124 normal/sanitized native/portrait paper modal and Home cases passed')

for san in (False,True):
 target=OUT/f'home-{int(san)}'
 flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',*flags,'-I'+str(ROOT/'lib/PortableApps/include'),str(ROOT/'test/native_apps/portable_home_test.c'),'-o',str(target)],check=True)
 subprocess.run([str(target)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))

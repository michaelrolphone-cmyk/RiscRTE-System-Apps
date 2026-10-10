#!/usr/bin/env python3
"""Opt-in flipped paper Clock/launcher: real QuickActions, Home, alarm and restore."""
from pathlib import Path
import os,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='reader-flip-') as tmp:
 for san in (False,True):
  for landscape in (False,True):
   for launcher in (False,True):
    flags=['-DPORTABLE_PAPER_PREFERENCES','-DTEST_READER_FLIP','-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_RTC_WALL_TIME','-DPORTABLE_INPUT_NAVIGATION']
    flags+=['-DTEST_SPRINGBOARD','-DPORTABLE_RETURN_APP="parent.elf"','-DPORTABLE_HOME_APP="default.elf"'] if launcher else ['-DPORTABLE_APP_OWNS_TOUCH_CHROME','-DPORTABLE_CROWN_SLEEP_UNAVAILABLE']
    if landscape:flags+=['-DTEST_NATIVE_LANDSCAPE','-DPORTABLE_DISPLAY_ROTATION=90']
    if san:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
    target=Path(tmp)/'paper'
    sources=[ROOT/'Apps'/('springboard.c' if launcher else 'paper_clock.c'),ROOT/'test/native_apps/paper_quick_test.c']
    sources += [ROOT/'lib/PortableApps/src'/p for p in ['adapter.c','quick_actions.c','quick_render.c','quick_session.c']]
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-Wall','-Wextra','-Werror',*flags,'-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),*map(str,sources),'-o',str(target)],check=True)
    for case in (range(20,28) if launcher else [*range(19),30,31,32,33]):
     subprocess.run([str(target),str(case)],check=True,timeout=20,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
print('124 flipped normal/sanitized native/portrait Clock and launcher modal/Home cases passed')

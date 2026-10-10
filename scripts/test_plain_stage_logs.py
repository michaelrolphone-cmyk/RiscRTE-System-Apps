#!/usr/bin/env python3
"""Production paper Clock/adapter: automatic readable stage statements."""
import os
import subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/plain-stage-logs'
OUT.mkdir(parents=True,exist_ok=True)
for sanitized in (False,True):
    for landscape in (False,True):
        flags=['-DPORTABLE_STAGE_LOGS','-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_ALARM_CLIENT',
               '-DPORTABLE_RTC_WALL_TIME','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_APP_OWNS_TOUCH_CHROME']
        if landscape:flags+=['-DTEST_NATIVE_LANDSCAPE','-DPORTABLE_DISPLAY_ROTATION=90']
        if sanitized:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
        binary=OUT/f'plain-{sanitized}-{landscape}'
        sources=[ROOT/'Apps/paper_clock.c',ROOT/'test/native_apps/plain_stage_log_test.c']+[ROOT/'lib/PortableApps/src'/name for name in ('adapter.c','quick_actions.c','quick_render.c','quick_session.c')]
        subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,
                        '-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),
                        *map(str,sources),'-o',binary],check=True)
        subprocess.run([binary],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'),timeout=20)
print('Plain stage statements: normal/ASan+UBSan portrait/native PASS')

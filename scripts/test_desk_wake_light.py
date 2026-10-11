#!/usr/bin/env python3
"""Run actual sparse adapter start with stored light settings and wake causes."""
import argparse,os,shutil,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser();p.add_argument('--sdk',type=Path,required=True);p.add_argument('--runtime-sdk',type=Path,required=True);a=p.parse_args()
flags=['-DPORTABLE_DESK_CLOCK','-DPORTABLE_DESK_CLOCK_SPARSE_START','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_APP_SLEEP_LOCAL','-DPORTABLE_SLEEP_MANUAL_ONLY','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_DISPLAY_ROTATION=90','-DPORTABLE_RTC_WALL_TIME','-DPORTABLE_APP_OWNS_TOUCH_CHROME','-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_QUICK_RADIOS','-DPORTABLE_PAPER_TRANSITIONS','-DPORTABLE_DESK_WAKE_LIGHT','-DTEST_DESK_WAKE_LIGHT']
with tempfile.TemporaryDirectory() as temporary:
 out=Path(temporary);include=out/'include';shutil.copytree(root/'lib/PortableApps/include',include);shutil.copytree(root/'lib/PortableApps/time',out/'time')
 shutil.copyfile(a.runtime_sdk/'RiscRuntimeV1.h',include/'RiscRuntimeV1.h')
 for name in ('RiscDisplayOutputV1.h','RiscDisplayOutputPowerV1.h'):shutil.copyfile(a.sdk/name,include/name)
 sources=[root/'test/native_apps/sparse_clock_adapter_test.c',*[root/'lib/PortableApps/src'/n for n in ('quick_actions.c','quick_render.c','quick_session.c','quick_radios.c')]]
 for san in (False,True):
  extra=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
  exe=out/str(san);subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*extra,'-I'+str(include),'-I'+str(root/'lib/NativeApps/include'),'-I'+str(root/'lib/PortableApps/src'),*map(str,sources),'-o',str(exe)],check=True)
  for case in range(29,35):subprocess.run([str(exe),str(case)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
print('Interactive wake: saved on/off/nondefault light and failed restoration; timer/cold stay dark PASS normal and ASan/UBSan')

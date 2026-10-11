#!/usr/bin/env python3
"""Regression for actual sparse adapter storage after a telemetry tick."""
import argparse,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--candidate',type=Path,required=True);a=p.parse_args()
out=ROOT/'build/sparse-broadcast-storage';out.mkdir(parents=True,exist_ok=True)
flags='TEST_SPARSE_BROADCAST_STORAGE PORTABLE_DESK_CLOCK PORTABLE_DESK_CLOCK_SPARSE_START PORTABLE_ALARM_CLIENT PORTABLE_APP_SLEEP_LOCAL PORTABLE_INPUT_NAVIGATION PORTABLE_DISPLAY_ROTATION=90 PORTABLE_RTC_WALL_TIME PORTABLE_APP_OWNS_TOUCH_CHROME PORTABLE_QUICK_ACTIONS PORTABLE_QUICK_RADIOS ALARM_SERVICE_TAGGED_V2 PORTABLE_X4_IDLE_POLICY PORTABLE_LOW_BATTERY PORTABLE_BLE_BROADCAST PORTABLE_BLE_BROADCAST_DEFAULT_OFF'.split()
for san in (False,True):
 extra=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
 exe=out/('sanitized' if san else 'normal')
 subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*extra,*['-D'+f for f in flags],'-I'+str(a.candidate/'idle-sdk/include'),'-I'+str(ROOT/'lib/NativeApps/include'),str(ROOT/'test/native_apps/sparse_clock_adapter_test.c'),*[str(ROOT/'lib/PortableApps/src'/n) for n in ('quick_actions.c','quick_session.c','quick_render.c','quick_radios.c')],'-o',str(exe)],check=True)
 subprocess.run([exe],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'),timeout=20)

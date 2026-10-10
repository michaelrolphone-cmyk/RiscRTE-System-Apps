#!/usr/bin/env python3
"""Production sparse adapter and app-owned active-work inhibition."""
import argparse,os,subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--candidate',type=Path,required=True);a=p.parse_args();out=r/'build/idle-policy/gate-tests';out.mkdir(parents=True,exist_ok=True)
for san in (False,True):
 common=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror']
 if san:common+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 for fixture,case in (('portable_wifi_test.c',61),('portable_update_app_test.c',40)):
  exe=out/(fixture+str(san));subprocess.run(['cc',*common,'-DTEST_IDLE_ELIGIBILITY','-Wno-misleading-indentation','-I'+str(r/'lib/PortableApps/include'),'-I'+str(r/'lib/NativeApps/include'),str(r/'test/native_apps'/fixture),'-o',str(exe)],check=True)
  subprocess.run([exe,str(case)],env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'),check=True)
 flags='PORTABLE_DESK_CLOCK PORTABLE_DESK_CLOCK_SPARSE_START PORTABLE_ALARM_CLIENT PORTABLE_APP_SLEEP_LOCAL PORTABLE_INPUT_NAVIGATION PORTABLE_DISPLAY_ROTATION=90 PORTABLE_RTC_WALL_TIME PORTABLE_APP_OWNS_TOUCH_CHROME PORTABLE_QUICK_ACTIONS PORTABLE_QUICK_RADIOS ALARM_SERVICE_TAGGED_V2 PORTABLE_X4_IDLE_POLICY PORTABLE_LOW_BATTERY'.split()
 exe=out/('sparse-'+str(san));sources=[r/'test/native_apps/x4_idle_sparse_test.c',*[r/'lib/PortableApps/src'/n for n in ('quick_actions.c','quick_session.c','quick_render.c','quick_radios.c')]]
 subprocess.run(['cc',*common,*['-D'+f for f in flags],'-I'+str(a.candidate/'idle-sdk/include'),'-I'+str(r/'lib/NativeApps/include'),*map(str,sources),'-o',str(exe)],check=True)
 for case in (1,2,3,4,900):subprocess.run([exe,str(case)],env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'),check=True)
print('14 normal/sanitizer network/update/cold/timer/held-wake/automatic-Light cases PASS')

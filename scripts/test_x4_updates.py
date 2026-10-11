#!/usr/bin/env python3
"""X4 policy and full shared service fault matrix, without a live feed."""
import argparse,os,subprocess
from pathlib import Path
parser=argparse.ArgumentParser();parser.add_argument('--ble-broadcast',action='store_true');parser.add_argument('--touch-scrolling',action='store_true');parser.add_argument('--target-dir',type=Path);args=parser.parse_args()
r=Path(__file__).resolve().parents[1];out=r/'build/x4-update-tests';out.mkdir(parents=True,exist_ok=True)
for sanitized in (False,True):
 flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
 common=[os.environ.get('CXX','c++'),'-std=c++17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',*flags,'-I'+str(r/'lib/PortableApps/include'),'-I'+str(r/'lib/NativeApps/include')]
 binary=out/f'policy-{sanitized}'
 subprocess.run([*common,str(r/'test/native_apps/x4_update_policy_test.cpp'),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
 binary=out/f'app-product-{sanitized}'
 subprocess.run([*common,str(r/'test/native_apps/x4_update_apps_product_test.cpp'),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
 binary=out/f'cohort-{sanitized}'
 subprocess.run([*common,'-DUPDATE_PRODUCT_X4','-DUPDATE_CATALOG_URL="https://fixture.invalid/x4"',str(r/'test/native_apps/portable_update_cohort_test.cpp'),'-o',str(binary)],check=True)
 for case in range(46):subprocess.run([str(binary),str(case)],check=True,timeout=20)
print('X4 disabled feed/product policy and 46 cohort fault cases normal + ASan/UBSan passed')

import portable_native_toolbar_build as native
include=(args.target_dir or r/'build/x4-updates')/'ota_update/native-time-sdk/include'
if not include.is_dir():raise SystemExit('Build explicit X4 update apps before native controller tests')
for firmware in (False,True):
 for sanitized in (False,True):
  flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
  if args.touch_scrolling:flags+=['-DPORTABLE_TOUCH_SCROLL','-DPORTABLE_APP_TOUCH_SCROLL','-DPORTABLE_UPDATE_TOUCH_SCROLL','-DPORTABLE_PAPER_TRANSITIONS']
  if args.ble_broadcast:flags+=['-DPORTABLE_BLE_BROADCAST','-DPORTABLE_BLE_BROADCAST_DEFAULT_OFF','-DPORTABLE_PAPER_PREFERENCES']
  binary=out/f'native-{firmware}-{sanitized}'
  subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-misleading-indentation',*flags,
   '-DPORTABLE_UPDATE_APP','-DPORTABLE_UPDATE_FIRMWARE='+str(int(firmware)),'-DPORTABLE_UPDATE_FEED_DISABLED',
   '-DUPDATE_RETURN_APP="springboard.elf"','-DPORTABLE_WIFI_INSTANCE=15u',
   '-DPORTABLE_NATIVE_TIME_TOOLBAR','-DPORTABLE_NATIVE_CUSTODY_FENCE','-DALARM_SERVICE_TAGGED_V2',
   '-DTEST_NATIVE_TOOLBAR_QUICK','-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_HOME_APP="default.elf"',
   '-I'+str(include),'-I'+str(r/'lib/NativeApps/include'),str(r/'test/native_apps/x4_update_native_test.c'),
   str(r/'test/native_apps/native_system_app_entry.c'),*[str(r/name) for name in native.SOURCES],
   *[str(r/'lib/PortableApps/src'/name) for name in ('quick_actions.c','quick_render.c','quick_session.c')],'-Wl,--wrap=free','-o',str(binary)],check=True)
  for case in ['no-feed','utc','release-retained','home','alarm-retained']+(['broadcast-check','broadcast-busy','broadcast-close-retained'] if args.ble_broadcast else []):
   subprocess.run([str(binary),case],check=True,timeout=20)
print('X4 OTA/App Store actual native paper controllers, UTC, alarm-v2 and retention passed')

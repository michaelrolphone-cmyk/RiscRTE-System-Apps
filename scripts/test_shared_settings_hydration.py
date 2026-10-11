#!/usr/bin/env python3
"""Real Wi-Fi, reader adapter and Home BUSY recovery; synthetic providers only."""
import os, shutil, subprocess, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
RUNTIME=Path(os.environ['RUNTIME_REPO']).resolve()
X4=Path(os.environ['X4_REPO']).resolve()
def run(command,**kw):return subprocess.run(list(map(str,command)),check=True,**kw)
with tempfile.TemporaryDirectory(prefix='shared-hydration-') as tmp:
 out=Path(tmp);inc=out/'include';shutil.copytree(ROOT/'lib/PortableApps/include',inc)
 shutil.copytree(ROOT/'lib/PortableApps/time',out/'time')
 for name in ('RiscRuntimeV1.h','RiscRealtimeV1.h','RiscProviderPromotionV1.h','RiscRetainedWakeV1.h'):
  shutil.copyfile(RUNTIME/'sdk/app'/name,inc/name)
 for name in ('RiscDisplayOutputV1.h','RiscDisplayOutputPowerV1.h','RiscTouchV1.h','RiscTouchPowerV1.h','RiscStorageVolumeV1.h'):
  shutil.copyfile(RUNTIME/'sdk/driver'/name,inc/name)
 includes=['-I'+str(inc),'-I'+str(ROOT/'lib/NativeApps/include'),'-I'+str(RUNTIME/'sdk/driver'),'-I'+str(X4/'minimal/drivers/x4pro_power')]
 common=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror']
 for san in (False,True):
  flags=['-fsanitize=undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'] if san else []
  wifi=out/'wifi'
  run([*common,*flags,'-DTEST_IDLE_ELIGIBILITY','-DTEST_WIFI_PAPER','-DPORTABLE_PAPER_PREFERENCES',*includes,ROOT/'test/native_apps/portable_wifi_test.c','-o',wifi])
  for case in (62,63):run([wifi,case])
  home=out/'home'
  names='adapter.c desk_clock_faces.c PortableRealtimeClient.c PortableTimeZone.c PortableTimeZoneCatalog.c PortableTimeZonePreference.c'.split()
  definitions=['-DTEST_NATIVE_LANDSCAPE','-DPORTABLE_DISPLAY_ROTATION=90','-DPORTABLE_APP_OWNS_TOUCH_CHROME','-DPORTABLE_RTC_WALL_TIME','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_APP_SLEEP_LOCAL','-DPORTABLE_CROWN_SLEEP_LOCAL','-DPORTABLE_SLEEP_MANUAL_ONLY','-DPORTABLE_DESK_CLOCK','-DPORTABLE_DESK_CLOCK_SPARSE_START','-DPORTABLE_DESK_LOCK_HOME','-DTEST_HOME_DESK_LOCK','-DPORTABLE_SHARED_SETTINGS_FIXTURE']
  run([*common,*flags,*definitions,*includes,ROOT/'test/native_apps/shared_settings_home_app.c',*[ROOT/'lib/PortableApps/src'/name for name in names],X4/'minimal/apps/portable_sleep.c',ROOT/'test/native_apps/sparse_clock_startup_test.c','-o',home])
  env=dict(os.environ,CLOCK_ZONE='America/Denver',CLOCK_FACE='2',HOME_FLIP='1',DESK_DIRECTION='1')
  images=[]
  for busy in (False,True):
   state=out/'state';state.unlink(missing_ok=True);pixels=out/'pixels'
   run([home,'foreground',state],env=dict(env,PAPER_FRAME=str(pixels),**({'SHARED_SETTINGS_BUSY':'1'} if busy else {})))
   images.append(pixels.read_bytes())
  assert images[0]==images[1], 'BUSY boot must converge to restored foreground preferences'
  # Retained timer bypass must never read live settings, even when poisoned.
  state=out/'timer';state.unlink(missing_ok=True)
  run([home,'terminal',state],env=dict(env,SHARED_SETTINGS_BUSY='1'))
print('Shared hydration normal/UBSan: Wi-Fi draft-safe reload, reader flip, Home format/zone/basis/direction, cached-value preservation and timer KV bypass PASS')

#!/usr/bin/env python3
"""Actual cold/reset animation, touch cancellation and unchanged Home endpoint."""
import argparse,json,os,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for name in ('runtime-sdk','driver-sdk','sleep-source','utilities'):p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--tagged-alarm',action='store_true',help='Use the exact deployed alarm.service@2 descriptor')
p.add_argument('--output-dir',type=Path,default=ROOT/'build/boot-animation-startup')
a=p.parse_args();out=a.output_dir;out.mkdir(parents=True,exist_ok=True)
include=out/'include';shutil.copytree(ROOT/'lib/PortableApps/include',include,dirs_exist_ok=True);shutil.copytree(ROOT/'lib/PortableApps/time',out/'time',dirs_exist_ok=True)
for name in ('RiscRuntimeV1.h','RiscRealtimeV1.h','RiscProviderPromotionV1.h','RiscRetainedWakeV1.h'):shutil.copyfile(a.runtime_sdk/name,include/name)
for name in ('RiscDisplayOutputV1.h','RiscDisplayOutputPowerV1.h','RiscDisplayOutputMetricsV1.h','RiscDisplayOutputSnapshotV1.h','RiscTouchV1.h','RiscTouchPowerV1.h','RiscStorageVolumeV1.h'):shutil.copyfile(a.driver_sdk/name,include/name)
for name in ('AlarmRecords.h','PointsRecords.h','PointsSchedule.h','PointsUtcSchedule.h')+(('AlarmServiceV1.h','AlarmServiceV2.h') if a.tagged_alarm else ()):
 (include/name).write_bytes((a.utilities/'lib/Alarm/include'/name).read_bytes())
flags=['-DTEST_NATIVE_LANDSCAPE','-DPORTABLE_DISPLAY_ROTATION=90','-DPORTABLE_APP_OWNS_TOUCH_CHROME','-DPORTABLE_RTC_WALL_TIME','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_APP_SLEEP_LOCAL','-DPORTABLE_CROWN_SLEEP_LOCAL','-DPORTABLE_SLEEP_MANUAL_ONLY','-DPORTABLE_DESK_CLOCK','-DPORTABLE_DESK_CLOCK_SPARSE_START','-DPORTABLE_HOME_POINTS_NATIVE_UTC','-DALARM_NATIVE_UTC','-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_QUICK_RADIOS','-DPORTABLE_PAPER_TRANSITIONS','-DPORTABLE_PAPER_CROSSFADE']
if a.tagged_alarm:flags+=['-DALARM_SERVICE_TAGGED_V2']
flags+=['-DPORTABLE_RASTER_SNAPSHOT','-DPORTABLE_DESK_LOCK_HOME','-DPORTABLE_BOOT_ANIMATION','-DPORTABLE_STAGE_LOGS']
results=[]
names=['adapter.c','desk_clock_faces.c','PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c','quick_actions.c','quick_render.c','quick_session.c','quick_radios.c']
for sanitized in (False,True):
 sf=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
 binary=out/f'boot-{int(sanitized)}'
 subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*sf,*flags,
  '-I'+str(include),'-I'+str(ROOT/'lib/NativeApps/include'),'-I'+str(a.runtime_sdk.parent/'driver'),'-I'+str(a.sleep_source.parent.parent/'drivers/x4pro_power'),
  str(ROOT/'Apps/paper_clock.c'),*[str(ROOT/'lib/PortableApps/src'/name) for name in names],str(a.sleep_source),str(ROOT/'test/native_apps/boot_animation_startup_test.c'),'-o',str(binary)],check=True)
 images=[]
 for case,skip in [('cold',0),('reset',0),('cold',1),('cold-reload',0),('reset-reload',0),('home-ready',0)]:
  directory=out/f'{case}-{skip}-{int(sanitized)}';directory.mkdir(exist_ok=True);state=directory/'state.bin'
  result=subprocess.run([binary,case,state,directory,str(skip)],env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',RAW_ASYNC='1',CLOCK_EPOCH='1791369720'),text=True,capture_output=True,timeout=30)
  if result.returncode:raise AssertionError((case,skip,sanitized,result.stdout,result.stderr))
  print(result.stdout.strip(),flush=True);results.append({'case':case,'skip':skip,'sanitized':sanitized,'result':result.stdout.strip()})
  images.append((directory/'home.bin').read_bytes())
 assert all(image==images[-1] for image in images),'Animation changed final Home image'
(out/'evidence.json').write_text(json.dumps({'hardware_tested':False,'cases':results},indent=2)+'\n')
print('Actual Clock boot animation, repeated-boot isolation, input cancellation and Home endpoint PASS')

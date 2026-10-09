#!/usr/bin/env python3
"""Actual native Clock, adapter and Home Points; provider doubles, not hardware."""
import argparse, hashlib, json, os, shutil, subprocess
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--utilities',type=Path,required=True)
p.add_argument('--runtime-sdk',type=Path,required=True)
p.add_argument('--driver-sdk',type=Path,required=True)
p.add_argument('--sleep-source',type=Path,required=True)
p.add_argument('--xtensa-cc',required=True)
a=p.parse_args()
out=ROOT/'build/home-points';out.mkdir(parents=True,exist_ok=True)
include=out/'include';shutil.copytree(ROOT/'lib/PortableApps/include',include,dirs_exist_ok=True)
shutil.copytree(ROOT/'lib/PortableApps/time',out/'time',dirs_exist_ok=True)
for name in ('RiscRuntimeV1.h','RiscRealtimeV1.h','RiscProviderPromotionV1.h','RiscRetainedWakeV1.h'):
 shutil.copyfile(a.runtime_sdk/name,include/name)
for name in ('RiscDisplayOutputV1.h','RiscDisplayOutputPowerV1.h','RiscTouchV1.h','RiscTouchPowerV1.h','RiscStorageVolumeV1.h'):
 shutil.copyfile(a.driver_sdk/name,include/name)
PIN='637e13b0bce62ad49b756bec2468a6271d163fc7'
for name in ('AlarmRecords.h','PointsRecords.h','PointsSchedule.h','PointsUtcSchedule.h'):
 (include/name).write_bytes(subprocess.check_output(['git','-C',a.utilities,'show',PIN+':lib/Alarm/include/'+name]))
flags=['-DTEST_NATIVE_LANDSCAPE','-DPORTABLE_DISPLAY_ROTATION=90','-DPORTABLE_APP_OWNS_TOUCH_CHROME',
 '-DPORTABLE_RTC_WALL_TIME','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_APP_SLEEP_LOCAL',
 '-DPORTABLE_CROWN_SLEEP_LOCAL','-DPORTABLE_SLEEP_MANUAL_ONLY','-DPORTABLE_DESK_CLOCK','-DPORTABLE_DESK_CLOCK_SPARSE_START',
 '-DPORTABLE_HOME_POINTS_NATIVE_UTC','-DALARM_NATIVE_UTC']
incs=['-I'+str(include),'-I'+str(ROOT/'lib/NativeApps/include'),'-I'+str(a.runtime_sdk.parent/'driver'),
 '-I'+str(a.sleep_source.parent.parent/'drivers/x4pro_power')]
names=['adapter.c','desk_clock_faces.c','PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c']
sources=[ROOT/'Apps/paper_clock.c',*[ROOT/'lib/PortableApps/src'/n for n in names],a.sleep_source,ROOT/'test/native_apps/sparse_clock_startup_test.c']
quick=[ROOT/'lib/PortableApps/src'/n for n in ('quick_actions.c','quick_render.c','quick_session.c','quick_radios.c')]
cases='home-ready home-custom home-empty home-invalid home-unavailable home-storage-error home-meta-invalid home-default home-acquire home-context home-release home-tap home-dial home-retry home-swipe home-cancel home-held home-minute terminal cold gpio invalid-record unset missing-zone missing-basis bad-zone bad-basis native-context native-release panel-retained retained resume-retained'.split()
checks=0
for san in (False,True):
 sf=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
 pure=out/('projection-'+str(int(san)))
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',*sf,'-DALARM_NATIVE_UTC',*incs,
  ROOT/'test/native_apps/paper_home_points_projection_test.c',ROOT/'lib/PortableApps/src/PortableTimeZone.c',ROOT/'lib/PortableApps/src/PortableTimeZoneCatalog.c','-o',pure],check=True)
 subprocess.run([pure],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
 for qa in (False,True):
  binary=out/f'controller-{int(san)}-{int(qa)}'
  subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*sf,*flags,*incs,
   *(['-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_QUICK_RADIOS'] if qa else []),*sources,*(quick if qa else []),'-o',binary],check=True)
  for case in [*cases,*(['home-top','home-top-left','home-top-held','home-top-cancel'] if qa else [])]:
   state=out/'state.bin';state.unlink(missing_ok=True)
   capture=out/(case+'.pixels')
   env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',CLOCK_EPOCH='1791369720',PAPER_FRAME=str(capture),HOME_PRESS_FRAME=str(out/(case+'-pressed.pixels')))
   if case=='home-minute':env['CLOCK_EPOCH']='1791369719'
   subprocess.run([binary,case,state],check=True,env=env,timeout=20,stdout=subprocess.DEVNULL);checks+=1
  asynchronous=['home-tap','home-dial','home-cancel','home-held']+(['home-top','home-top-left','home-top-held','home-top-cancel'] if qa else [])
  for case in asynchronous:
   state=out/'state.bin';state.unlink(missing_ok=True)
   subprocess.run([binary,case,state],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',CLOCK_EPOCH='1791369720',RAW_ASYNC='1'),timeout=20,stdout=subprocess.DEVNULL);checks+=1
  print(f'Home controller: sanitizer={san}, Quick Actions={qa}: sync + async + pressed pixels PASS',flush=True)
# Reviewed actual mono Home panels. Ignore the unrelated main Clock artwork.
golden=json.loads((ROOT/'test/native_apps/fixtures/home-points-pixels.json').read_text())
# Inspectable actual mono panel output, rotated back to logical portrait.
for case in ('home-ready','home-custom','home-empty','home-invalid','home-unavailable','home-default'):
 data=(out/(case+'.pixels')).read_bytes();assert len(data)==48000
 img=Image.frombytes('1',(800,480),data).point(lambda p:255-p).rotate(270,expand=True)
 img.save(out/(case+'.png'))
 assert img.crop((24,744,456,800)).getextrema()==(255,255),case+' instruction footer returned'
 assert hashlib.sha256(img.crop(tuple(golden['logical_crop'])).tobytes()).hexdigest()==golden['sha256'][case],case
subprocess.run(['python3',ROOT/'scripts/compare_home_reference.py','--current',out/'home-ready.png','--output',out/'comparison'],check=True)
# A failed launch still surfaces a real, actionable error notice.
retry=Image.frombytes('1',(800,480),(out/'home-retry.pixels').read_bytes()).point(lambda p:255-p).rotate(270,expand=True)
assert retry.crop((24,744,456,800)).getextrema()==(0,255)
# Pixel witnesses: source-backed populated panels differ from empty/unavailable,
# custom metadata changes the label; empty and corrupt records never look populated.
images={n:(out/(n+'.pixels')).read_bytes() for n in ('home-ready','home-custom','home-empty','home-invalid','home-unavailable')}
assert len(set(images.values()))==4 and images['home-invalid']==images['home-unavailable']
target=out/'target'
subprocess.run(['python3',ROOT/'scripts/build_paper_clock.py','--desk-clock','--sparse-start','--navigation','--alarm-client',
 '--sleep-capability','x4.power','--sleep-sdk',a.driver_sdk,'--retained-wake-sdk',a.runtime_sdk,
 '--local-sleep-source',a.sleep_source,'--tagged-alarm-utilities',a.utilities,'--quick-actions','--quick-radios','--output-dir',target],
 check=True,env=dict(os.environ,NATIVE_APP_CC=a.xtensa_cc))
receipt=json.loads((target/'build-evidence.json').read_text());assert receipt['home_points']['foreground_only']
(out/'evidence.json').write_text(json.dumps({'controller_process_cases':checks,'projection_cases':'native UTC, custom/end/empty/range/DST',
 'actual_pixel_states':list(images),'target':receipt,'hardware_validation':'not run'},indent=2)+'\n')
print(f'{checks} controller cases, actual mono pixels and target ELF validation PASS')

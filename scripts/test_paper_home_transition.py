#!/usr/bin/env python3
"""Production native Home and sparse boot with optional completed snapshots."""
import argparse,json,os,shutil,subprocess
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for name in ('runtime-sdk','driver-sdk','sleep-source','utilities','springboard-frame'):p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--output-dir',type=Path,default=ROOT/'build/paper-home-transition')
a=p.parse_args();out=a.output_dir;out.mkdir(parents=True,exist_ok=True)
include=out/'include';shutil.copytree(ROOT/'lib/PortableApps/include',include,dirs_exist_ok=True);shutil.copytree(ROOT/'lib/PortableApps/time',out/'time',dirs_exist_ok=True)
for name in ('RiscRuntimeV1.h','RiscRealtimeV1.h','RiscProviderPromotionV1.h','RiscRetainedWakeV1.h'):shutil.copyfile(a.runtime_sdk/name,include/name)
for name in ('RiscDisplayOutputV1.h','RiscDisplayOutputPowerV1.h','RiscDisplayOutputMetricsV1.h','RiscDisplayOutputSnapshotV1.h','RiscTouchV1.h','RiscTouchPowerV1.h','RiscStorageVolumeV1.h'):shutil.copyfile(a.driver_sdk/name,include/name)
for name in ('AlarmRecords.h','PointsRecords.h','PointsSchedule.h','PointsUtcSchedule.h'):
 (include/name).write_bytes(subprocess.check_output(['git','-C',a.utilities,'show','637e13b0bce62ad49b756bec2468a6271d163fc7:lib/Alarm/include/'+name]))
source=out/'springboard.bin';image=Image.open(a.springboard_frame).convert('1');assert image.size==(800,480)
source.write_bytes(bytes(b^255 for b in image.tobytes()))
flags=['-DTEST_NATIVE_LANDSCAPE','-DPORTABLE_DISPLAY_ROTATION=90','-DPORTABLE_APP_OWNS_TOUCH_CHROME','-DPORTABLE_RTC_WALL_TIME','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_APP_SLEEP_LOCAL','-DPORTABLE_CROWN_SLEEP_LOCAL','-DPORTABLE_SLEEP_MANUAL_ONLY','-DPORTABLE_DESK_CLOCK','-DPORTABLE_DESK_CLOCK_SPARSE_START','-DPORTABLE_HOME_POINTS_NATIVE_UTC','-DALARM_NATIVE_UTC','-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_QUICK_RADIOS','-DPORTABLE_PAPER_TRANSITIONS','-DPORTABLE_PAPER_CROSSFADE']
names=['adapter.c','desk_clock_faces.c','PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c','quick_actions.c','quick_render.c','quick_session.c','quick_radios.c']
for sanitized in (False,True):
 sf=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
 binary=out/f'home-{int(sanitized)}'
 subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*sf,*flags,
  '-I'+str(include),'-I'+str(ROOT/'lib/NativeApps/include'),'-I'+str(a.runtime_sdk.parent/'driver'),'-I'+str(a.sleep_source.parent.parent/'drivers/x4pro_power'),
  str(ROOT/'Apps/paper_clock.c'),*[str(ROOT/'lib/PortableApps/src'/name) for name in names],str(a.sleep_source),str(ROOT/'test/native_apps/paper_home_transition_test.c'),'-o',str(binary)],check=True)
 for case in ('home-ready','cold','terminal'):
  for extension in ('present','absent'):
   directory=out/f'{case}-{extension}-{int(sanitized)}'
   if directory.exists():shutil.rmtree(directory)
   directory.mkdir();state=directory/'state.bin'
   result=subprocess.run([binary,case,state,source,extension,directory],env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',RAW_ASYNC='1',CLOCK_EPOCH='1791369720'),text=True,capture_output=True,timeout=30)
   if result.returncode:raise AssertionError((case,extension,sanitized,result.stdout,result.stderr))
   print(result.stdout.strip(),flush=True)
   for path in directory.glob('*.pbm'):Image.open(path).rotate(270,expand=True).save(path.with_suffix('.png'))
 baseline=Image.open(out/f'home-ready-absent-{int(sanitized)}/frame-01.pbm').tobytes()
 frames=sorted((out/f'home-ready-present-{int(sanitized)}').glob('frame-*.pbm'))
 assert len(frames)>=5
 # 140 ms async transfers allow four fade submissions at 0,140,280,420 ms.
 assert Image.open(frames[3]).tobytes()==baseline
 assert len({Image.open(path).tobytes() for path in frames[:4]})==4
 logical=[Image.open(path).rotate(270,expand=True) for path in frames[:4]]
 logical[0].save(out/f'springboard-to-home-{int(sanitized)}.gif',save_all=True,append_images=logical[1:],duration=150,loop=0)
print('Native Home: real Points/font endpoint, reverse crossfade, cold logo and sparse timer isolation PASS')

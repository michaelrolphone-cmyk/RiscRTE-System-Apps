#!/usr/bin/env python3
"""Production native Home and sparse boot with optional completed snapshots."""
import argparse,csv,json,os,re,shutil,subprocess
from pathlib import Path
from PIL import Image,ImageChops
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for name in ('runtime-sdk','driver-sdk','sleep-source','utilities','springboard-frame'):p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--stage-logs',action='store_true',help='Qualify direct stage logs with motion and native Home controls')
p.add_argument('--tagged-alarm',action='store_true',help='Use the exact deployed alarm.service@2 descriptor')
p.add_argument('--snapshot',action='store_true',help='Exercise the deployed deferred Home raster path')
p.add_argument('--output-dir',type=Path,default=ROOT/'build/paper-home-transition')
a=p.parse_args();out=a.output_dir;out.mkdir(parents=True,exist_ok=True)
include=out/'include';shutil.copytree(ROOT/'lib/PortableApps/include',include,dirs_exist_ok=True);shutil.copytree(ROOT/'lib/PortableApps/time',out/'time',dirs_exist_ok=True)
for name in ('RiscRuntimeV1.h','RiscRealtimeV1.h','RiscProviderPromotionV1.h','RiscRetainedWakeV1.h'):shutil.copyfile(a.runtime_sdk/name,include/name)
for name in ('RiscDisplayOutputV1.h','RiscDisplayOutputPowerV1.h','RiscDisplayOutputMetricsV1.h','RiscDisplayOutputSnapshotV1.h','RiscTouchV1.h','RiscTouchPowerV1.h','RiscStorageVolumeV1.h'):shutil.copyfile(a.driver_sdk/name,include/name)
for name in ('AlarmRecords.h','PointsRecords.h','PointsSchedule.h','PointsUtcSchedule.h')+(('AlarmServiceV1.h','AlarmServiceV2.h') if a.tagged_alarm else ()):
 (include/name).write_bytes((a.utilities/'lib/Alarm/include'/name).read_bytes())
source=out/'springboard.bin';image=Image.open(a.springboard_frame).convert('1');assert image.size==(800,480)
source.write_bytes(bytes(b^255 for b in image.tobytes()))
flags=['-DTEST_NATIVE_LANDSCAPE','-DPORTABLE_DISPLAY_ROTATION=90','-DPORTABLE_APP_OWNS_TOUCH_CHROME','-DPORTABLE_RTC_WALL_TIME','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_APP_SLEEP_LOCAL','-DPORTABLE_CROWN_SLEEP_LOCAL','-DPORTABLE_SLEEP_MANUAL_ONLY','-DPORTABLE_DESK_CLOCK','-DPORTABLE_DESK_CLOCK_SPARSE_START','-DPORTABLE_HOME_POINTS_NATIVE_UTC','-DALARM_NATIVE_UTC','-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_QUICK_RADIOS','-DPORTABLE_PAPER_TRANSITIONS','-DPORTABLE_PAPER_CROSSFADE']
if a.stage_logs:flags+=['-DPORTABLE_STAGE_LOGS','-DPORTABLE_STAGE_DISPLAY_METRICS']
if a.tagged_alarm:flags+=['-DALARM_SERVICE_TAGGED_V2']
if a.snapshot:flags+=['-DPORTABLE_RASTER_SNAPSHOT','-DPORTABLE_DESK_LOCK_HOME']
results=[];baseline_file=out/'native-home.bin'
# The snapshot profile exercises foreground Home. Legacy cold/timer fixture
# entry/exit signals below belong to the separate sparse desk-only profile.
cases=('home-ready',) if a.snapshot else ('home-ready','cold','terminal')+(('home-quick-top','home-quick-swipe','home-quick-slow','home-quick-interrupt') if a.stage_logs else ())
names=['adapter.c','desk_clock_faces.c','PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c','quick_actions.c','quick_render.c','quick_session.c','quick_radios.c']
for sanitized in (False,True):
 sf=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
 binary=out/f'home-{int(sanitized)}'
 subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*sf,*flags,
  '-I'+str(include),'-I'+str(ROOT/'lib/NativeApps/include'),'-I'+str(a.runtime_sdk.parent/'driver'),'-I'+str(a.sleep_source.parent.parent/'drivers/x4pro_power'),
  str(ROOT/'Apps/paper_clock.c'),*[str(ROOT/'lib/PortableApps/src'/name) for name in names],str(a.sleep_source),str(ROOT/'test/native_apps/paper_home_transition_test.c'),'-o',str(binary)],check=True)
 for case in cases:
  for extension in ('present','absent'):
   directory=out/f'{case}-{extension}-{int(sanitized)}'
   if directory.exists():shutil.rmtree(directory)
   directory.mkdir();state=directory/'state.bin'
   result=subprocess.run([binary,case,state,source,extension,directory],env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',RAW_ASYNC='1',CLOCK_EPOCH='1791369720',NATIVE_HOME_BASELINE=str(baseline_file)),text=True,capture_output=True,timeout=30)
   if result.returncode:raise AssertionError((case,extension,sanitized,result.stdout,result.stderr))
   print(result.stdout.strip(),flush=True)
   results.append({'case':case,'extension':extension,'sanitized':sanitized,'result':result.stdout.strip()})
   for path in directory.glob('*.pbm'):Image.open(path).rotate(270,expand=True).save(path.with_suffix('.png'))
   if case=='home-ready' and extension=='absent':
    baseline_file.write_bytes(bytes(b^255 for b in Image.open(directory/'frame-01.pbm').convert('1').tobytes()))
   if case.startswith('home-quick-'):
    baseline=Image.open(out/f'home-ready-absent-{int(sanitized)}/frame-01.pbm')
    assert Image.open(directory/'restored.pbm').tobytes()==baseline.tobytes()
    last=int(re.search(r'restored_frame=(\d+)',result.stdout).group(1))
    frames=[Image.open(directory/f'frame-{i:02}.pbm').rotate(270,expand=True).convert('RGB') for i in range(1,last+1)]
    full=baseline.rotate(270,expand=True).convert('RGB')
    records=list(csv.DictReader((directory/'frames.csv').open()))
    controls=[int(record['frame']) for record in records if record['phase']=='controls' and int(record['frame'])<=last]
    underlay=frames[controls[0]-2]
    boxes=[ImageChops.difference(underlay,frames[index-1]).getbbox() for index in controls]
    assert sum(bool(box and 0<box[3]<745) for box in boxes)>=2,case+' lacks visible partial sheet positions'
    assert any(box and box[3]>=750 for box in boxes),case+' never covers the full Home'
    frames[0].save(directory/'motion.gif',save_all=True,append_images=frames[1:],duration=160,loop=0)
 baseline=Image.open(out/f'home-ready-absent-{int(sanitized)}/frame-01.pbm').tobytes()
 frames=sorted((out/f'home-ready-present-{int(sanitized)}').glob('frame-*.pbm'))
 if a.snapshot:
  # Deferred replay advances this fixture's synthetic per-call clock. Require
  # the complete endpoint, not a fixed count of obsolete intermediate frames.
  assert any(Image.open(path).tobytes()==baseline for path in frames)
 else:
  assert len(frames)>=5
  # 140 ms async transfers allow four fade submissions at 0,140,280,420 ms.
  assert Image.open(frames[3]).tobytes()==baseline
  assert len({Image.open(path).tobytes() for path in frames[:4]})==4
 logical=[Image.open(path).rotate(270,expand=True) for path in frames[:4]]
 logical[0].save(out/f'springboard-to-home-{int(sanitized)}.gif',save_all=True,append_images=logical[1:],duration=150,loop=0)
(out/'evidence.json').write_text(json.dumps({'hardware':'not run','snapshot':a.snapshot,'stage_logs':a.stage_logs,'alarm_api':2 if a.tagged_alarm else 1,'cases':results},indent=2)+'\n')
print('Native Home: deferred raster endpoint with/without inherited image PASS' if a.snapshot else 'Native Home: real Points/font endpoint, reverse crossfade, cold logo and sparse timer isolation PASS')

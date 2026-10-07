#!/usr/bin/env python3
"""Real Clock/adapter on fresh host processes, using exact canonical SDK prefixes."""
import argparse,os,shutil,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--sdk',type=Path,required=True);p.add_argument('--evidence-dir',type=Path);a=p.parse_args()
for sanitizer in (False,True):
 with tempfile.TemporaryDirectory(prefix='desk-clock-') as temporary:
  out=Path(temporary);include=out/'include';shutil.copytree(ROOT/'lib/PortableApps/include',include);shutil.copytree(ROOT/'lib/PortableApps/time',out/'time')
  for name in ('RiscDisplayOutputV1.h','RiscDisplayOutputPowerV1.h','RiscTouchV1.h','RiscTouchPowerV1.h','RiscStorageVolumeV1.h'):shutil.copyfile(a.sdk/name,include/name)
  flags=['-DTEST_NATIVE_LANDSCAPE','-DPORTABLE_DISPLAY_ROTATION=90','-DPORTABLE_APP_OWNS_TOUCH_CHROME','-DPORTABLE_RTC_WALL_TIME','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_APP_SLEEP_LOCAL','-DPORTABLE_CROWN_SLEEP_LOCAL','-DPORTABLE_SLEEP_MANUAL_ONLY','-DPORTABLE_DESK_CLOCK']
  if sanitizer:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
  exe=out/'clock';command=[os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',*flags,'-I'+str(include),'-I'+str(ROOT/'lib/NativeApps/include'),str(ROOT/'Apps/paper_clock.c'),str(ROOT/'lib/PortableApps/src/adapter.c'),str(ROOT/'lib/PortableApps/src/desk_clock_faces.c'),str(ROOT/'test/native_apps/paper_desk_clock_test.c'),'-o',str(exe)];subprocess.run(command,check=True)
  env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0')
  for case in range(44):
   state=out/f'case-{case}.bin'
   if case in (1,2,3,13,14,15,16,29,32):subprocess.run([str(exe),'0',str(state)],env=env,check=True,timeout=20)
   subprocess.run([str(exe),str(case),str(state)],env=env,check=True,timeout=20)
  if a.evidence_dir and not sanitizer:
   from PIL import Image
   a.evidence_dir.mkdir(parents=True,exist_ok=True)
   for face in range(6):
    for case,suffix in ((0,'24h'),(18,'12h')):
     capture=out/'capture.pbm'
     subprocess.run([str(exe),str(case),'-',str(face)],env=dict(env,PAPER_FRAME=str(capture)),check=True,timeout=20)
     Image.open(capture).transpose(Image.Transpose.ROTATE_270).save(a.evidence_dir/f'face-{face}-{suffix}.png')
  if a.evidence_dir and not sanitizer:
   from PIL import Image
   for case,label in ((22,'entry-refused'),(1,'midnight-resume')):
    capture=out/'extra.pbm';state=out/(label+'.bin')
    if case==1:subprocess.run([str(exe),'0',str(state)],env=env,check=True,timeout=20)
    subprocess.run([str(exe),str(case),str(state)],env=dict(env,PAPER_FRAME=str(capture)),check=True,timeout=20)
    Image.open(capture).transpose(Image.Transpose.ROTATE_270).save(a.evidence_dir/(label+'.png'))
  state12=out/'format12.bin'
  subprocess.run([str(exe),'18',str(state12)],env=env,check=True,timeout=20)
  subprocess.run([str(exe),'18',str(state12)],env=env,check=True,timeout=20)
  radio_exe=out/'radio-clock'
  radio_command=command[:-2]+['-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_QUICK_RADIOS']+[str(ROOT/'lib/PortableApps/src'/name) for name in ('quick_actions.c','quick_render.c','quick_session.c','quick_radios.c')]+['-o',str(radio_exe)]
  subprocess.run(radio_command,check=True)
  for case in (44,45,46):subprocess.run([str(radio_exe),str(case)],env=env,check=True,timeout=20)
  for face in range(6):
   state=out/f'face-{face}.bin'
   for cycle in range(62):subprocess.run([str(exe),'1' if cycle else '0',str(state),str(face)],env=env,check=True,timeout=20,stdout=subprocess.DEVNULL)
print('Real Clock: normal/sanitized boot, manual/timer, refused/retained, old-image pixels, six faces x62 fresh-process cycles passed')

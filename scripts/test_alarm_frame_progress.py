#!/usr/bin/env python3
"""Exercise actual foreground poll during alarm BUSY, with queued/active frames."""
import argparse, os, shutil, subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--runtime',type=Path,required=True)
p.add_argument('--utilities',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--expect-starvation',action='store_true')
a=p.parse_args();r=Path(__file__).resolve().parents[1];a.output.mkdir(parents=True,exist_ok=True)
inc=a.output/'include';shutil.copytree(r/'lib/PortableApps/include',inc,dirs_exist_ok=True)
shutil.copytree(r/'lib/PortableApps/time',a.output/'time',dirs_exist_ok=True)
for n in ('RiscRuntimeV1.h','RiscRealtimeV1.h','RiscRuntimeHostControlV1.h'):
 shutil.copyfile(a.runtime/'sdk/app'/n,inc/n)
for terminal in (False,True):
 for sanitized in (False,True):
  exe=a.output/f'frame-progress-{int(terminal)}-{int(sanitized)}'
  flags=['-DALARM_SERVICE_TAGGED_V2','-DPORTABLE_NATIVE_CUSTODY_FENCE','-DPORTABLE_NATIVE_TIME_TOOLBAR',
   '-DPORTABLE_ALARM_BUSY_DEFER','-DPORTABLE_RASTER_SNAPSHOT']
  if terminal:flags+=['-DPORTABLE_ALARM_TERMINAL_RETENTION']
  if sanitized:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,
   '-I'+str(inc),'-I'+str(a.utilities/'lib/Alarm/include'),'-I'+str(r/'lib/NativeApps/include'),
   str(r/'test/native_apps/alarm_frame_progress_test.c'),
   *[str(r/'lib/PortableApps/src'/n) for n in ('quick_actions.c','quick_render.c','quick_session.c')],
   '-Wl,--wrap=free','-o',str(exe)],check=True)
  for case in ('busy-queued','busy-hardware','busy-worker'):
   subprocess.run([str(exe),case,'original-starves' if a.expect_starvation else 'fixed'],check=True,
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'),timeout=20)
print('PASS: 12 foreground alarm/frame cases; normal and ASan/UBSan, both client variants')

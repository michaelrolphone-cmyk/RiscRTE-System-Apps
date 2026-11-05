#!/usr/bin/env python3
"""Selected alarm client defers sustained native BUSY without losing custody."""
import argparse,os,shutil,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--utilities',type=Path,required=True);p.add_argument('--output',type=Path);a=p.parse_args()
r=Path(__file__).resolve().parents[1];out=a.output or r/'build/alarm-busy-adapter';out.mkdir(parents=True,exist_ok=True)
inc=out/'include';shutil.copytree(r/'lib/PortableApps/include',inc,dirs_exist_ok=True);shutil.copytree(r/'lib/PortableApps/time',out/'time',dirs_exist_ok=True)
for n in ['RiscRuntimeV1.h','RiscRealtimeV1.h','RiscRuntimeHostControlV1.h']:shutil.copyfile(a.runtime/'sdk/app'/n,inc/n)
cases=['busy-cold','busy-mid','storage','rtc','visual','sound','step-retained','status-retained','copied-retained','quick-retained','ack-retained','stop-retained']
for terminal in (False,True):
 for sanitized in (False,True):
  target=out/f'adapter-{int(terminal)}-{int(sanitized)}';flags=['-DALARM_SERVICE_TAGGED_V2','-DPORTABLE_NATIVE_CUSTODY_FENCE','-DPORTABLE_NATIVE_TIME_TOOLBAR','-DPORTABLE_ALARM_BUSY_DEFER']
  if terminal:flags+=['-DPORTABLE_ALARM_TERMINAL_RETENTION']
  if sanitized:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-I'+str(inc),'-I'+str(a.utilities/'lib/Alarm/include'),'-I'+str(r/'lib/NativeApps/include'),str(r/'test/native_apps/portable_alarm_v2_adapter_test.c'),*[str(r/'lib/PortableApps/src'/n) for n in ['quick_actions.c','quick_render.c','quick_session.c']],'-Wl,--wrap=free','-o',str(target)],check=True)
  for case in cases+(['global-retained'] if terminal else []):subprocess.run([str(target),case],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'),timeout=30)
print('Alarm client: 1000 deferred ticks at startup/mid-reconciliation, visible real errors and terminal-custody regressions; both clients normal + ASan/UBSan PASS')

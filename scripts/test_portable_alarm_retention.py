#!/usr/bin/env python3
"""Selected API1 retention: production client/adapter and unchanged opt-out bytes."""
import os, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
out=ROOT/'build/alarm-retention';out.mkdir(parents=True,exist_ok=True)
for backgrounds in (False,True):
 for san in (False,True):
  exe=out/f'adapter-{int(backgrounds)}-{int(san)}'
  flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror']
  if backgrounds:flags+=['-DPORTABLE_BLE_BROADCAST','-DPORTABLE_CONTEXTS_CLIENT']
  if san:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
  flags+=['-I'+str(ROOT/p) for p in ('lib/PortableApps/include','lib/NativeApps/include')]
  sources=['Apps/settings.c','test/native_apps/portable_alarm_retention_test.c']+['lib/PortableApps/src/'+p for p in ('quick_actions.c','quick_session.c','quick_render.c')]
  subprocess.run([os.environ.get('CC','cc'),*flags,*[str(ROOT/p) for p in sources],'-o',str(exe)],check=True)
  for case in range(17):subprocess.run([str(exe),str(case)],check=True,timeout=10,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
print('API1 client + actual adapter: 68 plain/sanitized executions PASS')

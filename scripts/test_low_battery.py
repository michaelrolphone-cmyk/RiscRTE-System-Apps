#!/usr/bin/env python3
"""Production one-shot policy, shared controls, adapter and Settings regressions."""
from pathlib import Path
import os,subprocess
ROOT=Path(__file__).resolve().parents[1]
out=ROOT/'build/low-battery';out.mkdir(parents=True,exist_ok=True)
for san in (False,True):
 flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-DPORTABLE_LOW_BATTERY', '-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include')]
 if san:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 exe=out/f'policy-{int(san)}'
 sources=[ROOT/'test/native_apps/low_battery_test.c',*[ROOT/'lib/PortableApps/src'/n for n in ('quick_actions.c','quick_session.c','quick_radios.c')]]
 subprocess.run([os.environ.get('CC','cc'),*flags,*map(str,sources),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
for san in (False,True):
 for rotation in (0,180):
  flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-DPORTABLE_LOW_BATTERY','-DPORTABLE_QUICK_RADIOS','-DPORTABLE_SLEEP_SETTINGS','-DPORTABLE_NOVA_UI',f'-DPORTABLE_TOUCH_ROTATION={rotation}', '-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include')]
  if san:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
  exe=out/f'adapter-{rotation}-{int(san)}'
  sources=[ROOT/'Apps/settings.c',ROOT/'test/native_apps/low_battery_adapter_test.c',*[ROOT/'lib/PortableApps/src'/n for n in ('quick_actions.c','quick_session.c','quick_radios.c','quick_render.c')]]
  subprocess.run([os.environ.get('CC','cc'),*flags,*map(str,sources),'-o',str(exe)],check=True)
  for case in [0,1,2,3,4,5,7,8,9]:subprocess.run([str(exe),str(case)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))

#!/usr/bin/env python3
"""Optional client gate over real adapter Home/crown/return/QuickActions paths."""
from pathlib import Path
import os,subprocess
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/launch-guard';OUT.mkdir(parents=True,exist_ok=True)
for san in (False,True):
 for rotation in (0,180):
  exe=OUT/f'guard-{rotation}-{int(san)}'
  flags=['-DPORTABLE_NOVA_UI',f'-DPORTABLE_TOUCH_ROTATION={rotation}']
  if san:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),str(ROOT/'Apps/settings.c'),str(ROOT/'test/native_apps/launch_guard_test.c'),*[str(ROOT/'lib/PortableApps/src'/n) for n in ('quick_actions.c','quick_render.c','quick_session.c')],'-o',str(exe)],check=True)
  for case in range(9):subprocess.run([str(exe),str(case)],check=True,timeout=30,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
print('36 real-adapter launch guard scenarios passed')

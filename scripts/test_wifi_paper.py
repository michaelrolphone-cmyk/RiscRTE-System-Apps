#!/usr/bin/env python3
"""Production Wi-Fi controller/adapter on retaining MONO1; fake providers only."""
import os
from pathlib import Path
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/wifi-paper';OUT.mkdir(parents=True,exist_ok=True)
for san in (False,True):
 for geometry in (0,1,2):
  landscape=geometry==1
  flags=['-DTEST_WIFI_PAPER','-DPORTABLE_NOVA_UI','-DTEST_WIFI_HOME','-DPORTABLE_HOME_APP="default.elf"','-DPORTABLE_QUICK_ACTIONS']
  if geometry==2:flags+=['-DTEST_PAPER_WIDTH=400','-DTEST_PAPER_HEIGHT=600']
  if landscape:flags+=['-DTEST_WIFI_LANDSCAPE','-DPORTABLE_DISPLAY_ROTATION=90']
  if san:
   flags+=['-fsanitize='+os.environ.get('WIFI_SANITIZERS','address,undefined'),'-fno-sanitize-recover=all','-fno-omit-frame-pointer']
   if sys.platform!='darwin':flags+=['-no-pie']
  binary=OUT/f'wifi-{geometry}-{int(san)}'
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',*flags,'-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),str(ROOT/'test/native_apps/portable_wifi_test.c'),*[str(ROOT/'lib/PortableApps/src'/name) for name in ['quick_actions.c','quick_render.c','quick_session.c']],'-o',str(binary)],check=True)
  for case in [*range(48),*range(50,61)]:
   frames=OUT/f'frames-{geometry}-{int(san)}';frames.mkdir(exist_ok=True)
   subprocess.run([str(binary),str(case)],check=True,timeout=20,env=dict(os.environ,PORTABLE_WIFI_FRAME_DIR=str(frames)))
for san in (0,1):
 for frame in (OUT/f'frames-0-{san}').glob('*.ppm'):
  assert frame.read_bytes()==(OUT/f'frames-1-{san}'/frame.name).read_bytes(), frame.name
print('354 paper production controller/adapter scenarios passed (portrait/native rotation/400x600, normal/sanitized; portrait/native pixels identical)')

#!/usr/bin/env python3
"""Offline paper update/controller tests with real shared adapter and fake grants."""
import os,sys,subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[1];out=r/'build/update-paper';out.mkdir(parents=True,exist_ok=True)
for sanitizer in (False,True):
 for firmware,rotation in ((f,r) for f in (0,1) for r in (0,90)):
  binary=out/f'paper-{firmware}-{rotation}-{sanitizer}';frames=out/f'frames-{firmware}-{rotation}';frames.mkdir(exist_ok=True)
  flags=['-fsanitize='+os.environ.get('UPDATE_SANITIZERS','address,undefined'),'-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitizer else []
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-DTEST_IDLE_ELIGIBILITY','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',*flags,f'-DPORTABLE_UPDATE_FIRMWARE={firmware}',f'-DPORTABLE_DISPLAY_ROTATION={rotation}','-I'+str(r/'lib/PortableApps/include'),'-I'+str(r/'lib/NativeApps/include'),str(r/'test/native_apps/portable_update_paper_test.c'),'-o',str(binary)],check=True)
  for scenario in [*range(26),28,29,30,31,32,33,36,38,39,40,41,42,43,44]:
   subprocess.run([str(binary),str(scenario)],check=True,timeout=60,env={**os.environ,'PORTABLE_UPDATE_FRAME_DIR':str(frames)})
print('Paper OTA/App Store normal and '+os.environ.get('UPDATE_SANITIZERS','address,undefined')+' passed')

# Quick sheet uses the same foreground app; unsupported radios stay inert.
for firmware in (0,1):
 binary=out/f'quick-{firmware}'
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-DTEST_IDLE_ELIGIBILITY','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-DPORTABLE_QUICK_ACTIONS','-no-pie','-fsanitize='+os.environ.get('UPDATE_SANITIZERS','address,undefined'),f'-DPORTABLE_UPDATE_FIRMWARE={firmware}','-I'+str(r/'lib/PortableApps/include'),'-I'+str(r/'lib/NativeApps/include'),str(r/'test/native_apps/portable_update_paper_test.c'),*[str(r/'lib/PortableApps/src'/name) for name in ('quick_actions.c','quick_render.c','quick_session.c')],'-o',str(binary)],check=True)
 for scenario in (45,46):subprocess.run([str(binary),str(scenario)],check=True,timeout=30)
print('Update Quick Actions: inert ungranted Wi-Fi, modal Back and global Home passed')

#!/usr/bin/env python3
from pathlib import Path
import subprocess,os
ROOT=Path(__file__).resolve().parents[1];out=ROOT/'build/quick-radios';out.mkdir(parents=True,exist_ok=True)
for san in (False,True):
 flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-I'+str(ROOT/'lib/PortableApps/include')]
 if san:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 exe=out/f'radios-{int(san)}'
 sources=[ROOT/'test/native_apps/quick_radio_test.c',ROOT/'lib/PortableApps/src/quick_radios.c',ROOT/'lib/PortableApps/src/quick_actions.c']
 subprocess.run([os.environ.get('CC','cc'),*flags,*map(str,sources),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))

for san in (False,True):
 for fixture in ('portable_wifi_test','portable_update_app_test'):
  flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-DTEST_RADIO_POLICY','-DPORTABLE_NOVA_UI','-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include')]
  if san:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
  exe=out/(fixture+'-'+str(int(san)))
  subprocess.run([os.environ.get('CC','cc'),*flags,str(ROOT/'test/native_apps'/(fixture+'.c')),'-o',str(exe)],check=True)
  for case in (48,49):subprocess.run([str(exe),str(case)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))

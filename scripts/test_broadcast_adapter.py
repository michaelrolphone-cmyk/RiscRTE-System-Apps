#!/usr/bin/env python3
from pathlib import Path
import os,subprocess
ROOT=Path(__file__).resolve().parents[1];out=ROOT/'build/broadcast-adapter';out.mkdir(parents=True,exist_ok=True)
for san in (False,True):
 flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-DPORTABLE_BLE_BROADCAST','-DPORTABLE_QUICK_RADIOS','-DPORTABLE_LOW_BATTERY','-DPORTABLE_SLEEP_SETTINGS','-DPORTABLE_NOVA_UI','-DPORTABLE_TOUCH_ROTATION=0','-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include')]
 if san:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 exe=out/f'adapter-{int(san)}'
 sources=[ROOT/'Apps/settings.c',ROOT/'test/native_apps/broadcast_adapter_test.c',*[ROOT/'lib/PortableApps/src'/n for n in ('quick_actions.c','quick_session.c','quick_radios.c','quick_render.c')]]
 subprocess.run([os.environ.get('CC','cc'),*flags,*map(str,sources),'-o',str(exe)],check=True)
 for case in range(9):subprocess.run([str(exe),str(case)],check=True,timeout=30,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))

for san in (False,True):
 flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-I'+str(ROOT/'lib/PortableApps/include')]
 if san:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 exe=out/f'client-{int(san)}'
 subprocess.run([os.environ.get('CC','cc'),*flags,str(ROOT/'test/native_apps/broadcast_client_test.c'),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))

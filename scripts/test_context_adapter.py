#!/usr/bin/env python3
from pathlib import Path
import os,subprocess
root=Path(__file__).resolve().parents[1];out=root/'build/contexts';out.mkdir(parents=True,exist_ok=True)
for san in (False,True):
 flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-DPORTABLE_CONTEXTS_CLIENT','-DPORTABLE_CONTEXT_FACE_COUNT=34','-DPORTABLE_QUICK_RADIOS','-DPORTABLE_LOW_BATTERY','-DPORTABLE_SLEEP_SETTINGS','-DPORTABLE_NOVA_UI','-DPORTABLE_TOUCH_ROTATION=0','-I'+str(root/'lib/PortableApps/include'),'-I'+str(root/'lib/NativeApps/include')]
 if san:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 exe=out/f'adapter-{int(san)}'
 sources=[root/'Apps/settings.c',root/'test/native_apps/context_adapter_test.c',*[root/'lib/PortableApps/src'/n for n in ('quick_actions.c','quick_session.c','quick_radios.c','quick_render.c')]]
 subprocess.run([os.environ.get('CC','cc'),*flags,*map(str,sources),'-o',str(exe)],check=True)
 for case in range(15):subprocess.run([str(exe),str(case)],check=True,timeout=30)

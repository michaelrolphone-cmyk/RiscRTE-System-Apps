#!/usr/bin/env python3
import os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];out=ROOT/'build/nova-ui';out.mkdir(parents=True,exist_ok=True)
incs=['-I'+str(ROOT/p) for p in ['lib/PortableApps/include','lib/NativeApps/include']]
catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
for san in (False,True):
 flags=['-DPORTABLE_NOVA_UI']+(['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else [])
 common=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*incs]
 binary=out/f'primitives-{int(san)}';frames=out/f'frames-{int(san)}';frames.mkdir(exist_ok=True)
 subprocess.run([*common,str(ROOT/'test/native_apps/nova_ui_test.c'),str(ROOT/'lib/PortableApps/src/adapter.c'),str(catalog),'-o',str(binary)],check=True)
 subprocess.run([str(binary),str(frames)],check=True)
 binary=out/f'touch-chrome-{int(san)}'
 subprocess.run([*common,'-DPORTABLE_APP_OWNS_TOUCH_CHROME',str(ROOT/'test/native_apps/nova_touch_chrome_test.c'),str(ROOT/'lib/PortableApps/src/adapter.c'),str(catalog),'-o',str(binary)],check=True)
 subprocess.run([str(binary),str(frames)],check=True)
 binary=out/f'alarms-{int(san)}'
 subprocess.run([*common,str(ROOT/'Apps/settings.c'),str(ROOT/'test/native_apps/portable_alarm_test.c'),'-o',str(binary)],check=True)
 for case in range(11):subprocess.run([str(binary),str(case)],check=True,timeout=20)
print('Nova primitives, 95 keyboard characters and 22 retained-modal scenarios passed')

#!/usr/bin/env python3
import os,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-DPORTABLE_CONTEXTS_CLIENT','-DPORTABLE_BLE_BROADCAST',
 '-DPORTABLE_QUICK_RADIOS','-DPORTABLE_LOW_BATTERY','-DPORTABLE_SLEEP_SETTINGS','-DPORTABLE_NOVA_UI',
 '-DPORTABLE_TOUCH_ROTATION=0','-I'+str(root/'lib/PortableApps/include'),'-I'+str(root/'lib/NativeApps/include')]
sources=[root/'Apps/settings.c',root/'test/native_apps/runtime_context_catalog_test.c',
 *[root/'lib/PortableApps/src'/name for name in ('quick_actions.c','quick_session.c','quick_render.c','quick_radios.c')]]
with tempfile.TemporaryDirectory(prefix='context-runtime-catalog-') as tmp:
 for sanitized in (False,True):
  exe=Path(tmp)/'test';san=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
  subprocess.run([os.environ.get('CC','cc'),*flags,*san,*map(str,sources),'-o',str(exe)],check=True)
  for mode in range(5):subprocess.run([str(exe),str(mode)],check=True,timeout=30,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
print('PASS 10 actual Contexts launch cases: long canonical route via native ID, short route unchanged, unavailable, absent, changed generation')

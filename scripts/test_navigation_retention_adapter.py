#!/usr/bin/env python3
"""Actual adapter hook boundaries; real bridge qualification remains separate."""
import os,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
base=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-DPORTABLE_CONTEXTS_CLIENT','-DPORTABLE_BLE_BROADCAST','-DPORTABLE_QUICK_RADIOS','-DPORTABLE_LOW_BATTERY','-DPORTABLE_SLEEP_SETTINGS','-DPORTABLE_NOVA_UI','-DPORTABLE_TOUCH_ROTATION=0','-I'+str(root/'lib/PortableApps/include'),'-I'+str(root/'lib/NativeApps/include')]
sources=[root/'Apps/settings.c',root/'test/native_apps/navigation_retention_adapter_test.c',*[root/'lib/PortableApps/src'/n for n in ('quick_actions.c','quick_session.c','quick_radios.c','quick_render.c')]]
with tempfile.TemporaryDirectory(prefix='navigation-retention-adapter-') as tmp:
 for sanitized in (False,True):
  flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
  exe=Path(tmp)/str(sanitized)
  subprocess.run([os.environ.get('CC','cc'),*base,*flags,*map(str,sources),'-o',str(exe)],check=True)
  for fault in range(10):
   result=subprocess.run([str(exe),str(fault)],capture_output=True,text=True,timeout=20)
   if result.returncode and 'LeakSanitizer does not work under ptrace' in result.stderr:
    env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0'
    result=subprocess.run([str(exe),str(fault)],capture_output=True,text=True,timeout=20,env=env)
   if result.returncode:raise RuntimeError(result.stdout+result.stderr)
   print(result.stdout.strip(),flush=True)
print('Local navigation retained-query adapter: 20 normal + ASan/UBSan boundary cases PASS')

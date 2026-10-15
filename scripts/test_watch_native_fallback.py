#!/usr/bin/env python3
"""Static fallback app_main must use native geometry and native touch together."""
from pathlib import Path
import os,subprocess,json
root=Path(__file__).resolve().parents[1];out=root/'build/watch-native-fallback';out.mkdir(parents=True,exist_ok=True)
flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-DPORTABLE_NOVA_UI','-DPORTABLE_WATCH_NATIVE_HEIGHT','-DPORTABLE_FORCE_FULL_FRAMES','-DPORTABLE_RETAINED_RGB565_HANDOFF','-DPORTABLE_HANDOFF_EAGER_MS=60','-DPORTABLE_ASYNC_RGB565','-DPORTABLE_CATALOG_LIMIT=23','-DPORTABLE_RETURN_APP="clock.elf"','-I'+str(root/'lib/PortableApps/include'),'-I'+str(root/'lib/NativeApps/include')]
results=[]
for sanitized in (False,True):
 san=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
 obj=out/f'adapter-{int(sanitized)}.o';exe=out/f'fallback-{int(sanitized)}'
 subprocess.run(['cc',*flags,*san,'-Dt5_storage_get_api=fallback_underlying_storage_get_api','-c',str(root/'lib/PortableApps/src/adapter.c'),'-o',str(obj)],check=True)
 subprocess.run(['cc',*flags,*san,str(root/'test/native_apps/watch_native_fallback_test.c'),str(root/'test/native_apps/fixtures/watch_springboard_catalog_23.c'),str(obj),'-o',str(exe)],check=True)
 for width,height in ((240,240),(410,502)):
  for provider in ('sync','async'):
   for metadata in ('slow-refresh','slow-latency','retains-image'):
    run=subprocess.run([str(exe),str(width),str(height),provider,metadata],text=True,capture_output=True,timeout=60)
    if run.returncode:raise RuntimeError(run.stdout+run.stderr)
    print(run.stdout,end='',flush=True);results.append(dict(width=width,height=height,provider=provider,metadata=metadata,sanitized=sanitized,result=run.stdout.strip()))
(out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
print(f'Native fallback: {len(results)} full-app cases, each verifies launch/edit/page, PASS')

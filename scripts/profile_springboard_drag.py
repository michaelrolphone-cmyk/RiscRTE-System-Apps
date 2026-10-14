#!/usr/bin/env python3
"""Profile the actual launcher by stage. Host CPU timings are not hardware FPS."""
import argparse,json,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source',type=Path);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
r=Path(__file__).resolve().parents[1];s=(a.source or r).resolve();a.output.mkdir(parents=True,exist_ok=True)
exe=a.output/'profile'
flags=['-DPORTABLE_NOVA_UI','-DPORTABLE_FORCE_FULL_FRAMES','-DPORTABLE_ASYNC_RGB565','-DPORTABLE_CATALOG_LIMIT=23','-DPORTABLE_WATCH_NATIVE_HEIGHT']
if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O2','-g','-Wall','-Wextra','-Werror',*flags,'-DSPRINGBOARD_SOURCE="'+str(s/'Apps/springboard.c')+'"','-I'+str(s/'lib/PortableApps/include'),'-I'+str(s/'lib/NativeApps/include'),str(r/'test/native_apps/springboard_drag_profile_test.c'),str(s/'lib/PortableApps/src/adapter.c'),str(r/'test/native_apps/fixtures/watch_springboard_catalog_23.c'),'-o',str(exe)],check=True)
results=[]
for w,h in [(240,240),(410,502)]:
 result=json.loads(subprocess.check_output([str(exe),str(w),str(h)],env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'),text=True));results.append(result);print(json.dumps(result),flush=True)
(a.output/'results.json').write_text(json.dumps(results,indent=2)+'\n')

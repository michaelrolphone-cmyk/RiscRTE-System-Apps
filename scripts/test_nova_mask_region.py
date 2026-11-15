#!/usr/bin/env python3
from pathlib import Path
import argparse,subprocess,json,os
r=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();o=a.output.resolve();o.mkdir(parents=True,exist_ok=True)
c=['cc','-std=c11','-O1','-g1','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-misleading-indentation','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-DPORTABLE_NOVA_UI','-DPORTABLE_WATCH_NATIVE_HEIGHT','-DPORTABLE_NOVA_MASK_BOUNDS','-DPORTABLE_TOUCH_SCROLL','-I'+str(r/'lib/PortableApps/include'),'-I'+str(r/'lib/NativeApps/include'),str(r/'test/native_apps/nova_mask_region_test.c'),'-o',str(o/'test')]
if a.sanitize:c+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
(o/'command.json').write_text(json.dumps(c,indent=2));v=subprocess.run(c,capture_output=True,text=True);(o/'compile.log').write_text(v.stdout+v.stderr);assert not v.returncode,v.stderr
v=subprocess.run([str(o/'test')],capture_output=True,text=True,env=os.environ|{'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'});(o/'result.log').write_text(v.stdout+v.stderr);assert not v.returncode,v.stdout+v.stderr;print(v.stdout)

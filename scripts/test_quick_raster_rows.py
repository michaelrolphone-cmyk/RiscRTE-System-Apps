#!/usr/bin/env python3
"""Compare unchanged full Quick rendering with whole/sliced candidate output."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--baseline',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
source=a.baseline.resolve()/'lib/PortableApps/src/quick_render.c';assert source.is_file()
flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(source.parent)]
if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
cc=os.environ.get('CC','cc');commands=[
 [cc,*flags,'-Dpqa_render=baseline_pqa_render','-Dpqa_paper_icon=baseline_pqa_paper_icon','-c',str(source),'-o',str(out/'baseline.o')],
 [cc,*flags,'-DPORTABLE_RASTER_SNAPSHOT',str(ROOT/'lib/PortableApps/src/quick_actions.c'),str(ROOT/'lib/PortableApps/src/quick_render.c'),str(ROOT/'test/native_apps/quick_rows_test.c'),str(out/'baseline.o'),'-o',str(out/'test')]]
for command in commands:subprocess.run(command,check=True,timeout=120)
result=subprocess.run([str(out/'test')],check=True,text=True,capture_output=True,timeout=120,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
print(result.stdout,end='');(out/'result.json').write_text(json.dumps({'commands':commands,'sanitized':a.sanitize,'result':result.stdout,'baseline_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'source_sha256':{str(path.relative_to(ROOT)):hashlib.sha256(path.read_bytes()).hexdigest() for path in [ROOT/'lib/PortableApps/src/quick_render.c',ROOT/'test/native_apps/quick_rows_test.c']}},indent=2)+'\n')

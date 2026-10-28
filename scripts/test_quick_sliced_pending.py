#!/usr/bin/env python3
"""Actual shared input reducer/dispatch under pending save; provider media doubled."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--alarm-sdk',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
 out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
 inc=out/'include';shutil.copytree(ROOT/'lib/PortableApps/include',inc);shutil.copytree(ROOT/'lib/PortableApps/time',out/'time')
 origins={str(inc/x.name):str(x) for x in (ROOT/'lib/PortableApps/include').glob('*.h')}
 for src in [*(a.runtime/'sdk/app').glob('*.h'),a.alarm_sdk/'AlarmServiceV1.h',a.alarm_sdk/'AlarmServiceV2.h']:
  shutil.copyfile(src,inc/src.name);origins[str(inc/src.name)]=str(src)
 flags=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-DPORTABLE_RASTER_SNAPSHOT','-DPORTABLE_QUICK_SAFE_CONTENT','-DPORTABLE_QUICK_CAPABILITY_SAFE','-DPORTABLE_WATCH_NATIVE_HEIGHT','-I'+str(inc),'-I'+str(ROOT/'lib/NativeApps/include')]
 if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 sources=[ROOT/'test/native_apps/quick_sliced_pending_test.c',*[ROOT/'lib/PortableApps/src'/n for n in ['quick_actions.c','quick_render.c','quick_session.c']]]
 deps=set(sources+[Path(__file__).resolve()])
 for src in sources:
  raw=subprocess.check_output([*flags,'-MM',str(src)],text=True)
  deps.update(Path(x).resolve() for x in raw.replace('\\\n',' ').split(':',1)[1].split())
 deps.update(Path(origins[str(x)]) for x in list(deps) if str(x) in origins)
 hashes={str(x):sha(x) for x in sorted(deps)};exe=out/'test';cmd=[*flags,*map(str,sources),'-o',str(exe)]
 r=subprocess.run(cmd,capture_output=True,text=True);(out/'compile.log').write_text(r.stdout+r.stderr);r.check_returncode()
 rows=[]
 for mode in range(13):
  r=subprocess.run([str(exe),str(mode)],capture_output=True,text=True,timeout=30,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
  (out/f'case-{mode}.log').write_text(r.stdout+r.stderr);r.check_returncode();rows.append({'case':mode,'passed':True});print(r.stdout,end='',flush=True)
 assert hashes=={str(x):sha(x) for x in sorted(deps)},'Sources changed during test'
 (out/'receipt.json').write_text(json.dumps({'passed':True,'sanitized':a.sanitize,'runs':rows,'source_sha256':hashes,'header_origins':origins,'command':cmd,'executable_sha256':sha(exe),'scope':'Actual shared adapter/raw reducer; provider media doubled. No product Runtime, target or device qualification.'},indent=2)+'\n')
if __name__=='__main__':main()

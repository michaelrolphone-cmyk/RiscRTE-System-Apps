#!/usr/bin/env python3
"""Opt-in provider-content Quick geometry; omitted profiles remain historical."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
files=['lib/PortableApps/src/quick_actions.c','lib/PortableApps/src/quick_render.c','test/native_apps/quick_safe_content_test.c']
lanes=[]
for san in (False,True):
 out=a.output/('sanitized' if san else 'normal');out.mkdir(exist_ok=True)
 cmd=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-pedantic','-DPORTABLE_WATCH_NATIVE_HEIGHT','-DPORTABLE_RASTER_SNAPSHOT','-DPORTABLE_QUICK_SAFE_CONTENT','-DPORTABLE_QUICK_CAPABILITY_SAFE','-I'+str(ROOT/'lib/PortableApps/include'),*[str(ROOT/f) for f in files],'-o',str(out/'test')]
 if san:cmd+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 subprocess.run(cmd,check=True);r=subprocess.run([str(out/'test'),str(out)],check=True,text=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
 # Exact production raster locks SAVE NOT CONFIRMED (not a false non-write claim).
 pending_hash=hashlib.sha256((out/'quick-safe-6.ppm').read_bytes()).hexdigest()
 assert pending_hash=='f14c792f26ac1ba0555e8e4f2eb6a4e6552954038ee3cb8307c01485f8fcea8a', 'pending caption raster changed'
 lanes.append({'sanitized':san,'command':cmd,'stdout':r.stdout,'pending_caption':'SAVE NOT CONFIRMED','pending_raster_sha256':pending_hash});print(r.stdout,end='')
receipt={'source_revision':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'source_dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True).strip()),'hardware_verified':False,'lanes':lanes}
receipt['inputs']={str(f.relative_to(ROOT)):hashlib.sha256(f.read_bytes()).hexdigest() for f in (ROOT/'lib/PortableApps').rglob('*') if f.is_file()}
(a.output/'evidence.json').write_text(json.dumps(receipt,indent=2)+'\n')

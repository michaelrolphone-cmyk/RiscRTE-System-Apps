#!/usr/bin/env python3
"""Verify native renderer/controller separately from full-adapter size admission."""
from pathlib import Path
import hashlib,json,os,subprocess
try:
 from PIL import Image
except ImportError:
 Image=None
root=Path(__file__).resolve().parents[1];out=root/'build/t064-render/native-raster';out.mkdir(parents=True,exist_ok=True)
results=[]
for sanitized in (False,True):
 binary=out/('sanitized' if sanitized else 'normal')
 flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
 command=[os.environ.get('CC','cc'),'-std=c11','-O1','-Wall','-Wextra','-Werror',*flags,'-I'+str(root/'lib/PortableApps/include'),'-I'+str(root/'lib/NativeApps/include'),str(root/'test/native_apps/springboard_native_raster_test.c'),str(root/'docs/t063/healthy-controller/catalog24.c'),'-o',str(binary)]
 subprocess.run(command,check=True)
 r=subprocess.run([str(binary),str(out)],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},capture_output=True,text=True,check=True,timeout=120);print(r.stdout,end='');(out/(binary.name+'.log')).write_text(r.stdout+r.stderr);results.append({'variant':binary.name,'result':r.stdout.strip()})
if Image is not None:
 for path in out.glob('*.ppm'):Image.open(path).save(path.with_suffix('.png'))
(out/'results.json').write_text(json.dumps({'scope':'native controller and raster only; full adapter size admission remains separately bounded','checks':results,'frames':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in out.glob('*.ppm')}},indent=2)+'\n')

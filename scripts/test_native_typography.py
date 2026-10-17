#!/usr/bin/env python3
"""Qualify production native glyph rasterization against independent pixels."""
import argparse, hashlib, json, os, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--quick',action='store_true');a=p.parse_args()
out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
s=(ROOT/'test/native_apps/portable_handoff_test.c').read_text().replace('#include "../../lib/PortableApps/src/adapter.c"','#include '+json.dumps(str(ROOT/'lib/PortableApps/src/adapter.c')))
s=s.replace('mock_pixels[240*243]','*mock_pixels').replace('sizeof(mock_pixels)','geometry_size').replace('.width=240,.height=240','.width=geometry_width,.height=geometry_height').replace('.stride_bytes=486,','.stride_bytes=geometry_stride,')
f=out/'fixture.c';f.write_text(s);results=[]
profiles={'generic':['-DPORTABLE_NATIVE_CANVAS'],'rf-alias':['-DPORTABLE_RF_NATIVE_CANVAS'],'subset':['-DPORTABLE_NATIVE_CANVAS','-DPORTABLE_NOVA_TEXT_FACES_012'],'watch-native':['-DPORTABLE_NATIVE_CANVAS','-DPORTABLE_WATCH_NATIVE_HEIGHT'],'scroll':['-DPORTABLE_NATIVE_CANVAS','-DPORTABLE_TOUCH_SCROLL']}
for profile,defines in profiles.items():
 for san in (False,True):
  if a.quick and (profile!='generic' or san):continue
  name=profile+('-sanitized' if san else '');folder=out/name;folder.mkdir(exist_ok=True);binary=out/(name+'.exe')
  flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
  cmd=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function',*flags,*defines,'-DPORTABLE_NOVA_UI','-DPORTABLE_RASTER_SNAPSHOT','-DPORTABLE_RETAINED_RGB565_HANDOFF','-DFRAME_FIXTURE='+json.dumps(str(f)),'-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),str(ROOT/'test/native_apps/native_typography_test.c'),'-o',str(binary)]
  subprocess.run(cmd,check=True);r=subprocess.run([binary,folder],text=True,capture_output=True,timeout=180,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
  (out/(name+'.log')).write_text(r.stdout+r.stderr);print(r.stdout+r.stderr,end='',flush=True);r.check_returncode()
  images={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in folder.glob('*.ppm')}
  results.append({'profile':profile,'sanitized':san,'output':r.stdout,'images_sha256':images})
  binary.unlink()
if not a.quick:
 assert results[0]['output']==results[1]['output']==results[2]['output']==results[3]['output'], 'Generic / RF alias / sanitizer pixels differ'
 for i in range(0,len(results),2):assert results[i]['output']==results[i+1]['output'] and results[i]['images_sha256']==results[i+1]['images_sha256']
paths=['lib/PortableApps/src/nova_native.inc','lib/PortableApps/src/adapter.c','lib/PortableApps/src/raster_snapshot_replay.inc','lib/PortableApps/include/PortableNativeCanvas.h','lib/PortableApps/include/PortableNovaUi.h','test/native_apps/native_typography_test.c','scripts/test_native_typography.py']
(out/'results.json').write_text(json.dumps({'hardware_verified':False,'results':results,'source_sha256':{p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in paths}},indent=2)+'\n')

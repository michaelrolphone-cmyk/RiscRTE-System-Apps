#!/usr/bin/env python3
"""Qualify real shared adapter geometry without rendering an intermediate view."""
import argparse, hashlib, json, os, subprocess
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args(); out = a.output.resolve(); out.mkdir(parents=True, exist_ok=True)
s = (ROOT/'test/native_apps/portable_handoff_test.c').read_text()
s = s.replace('#include "../../lib/PortableApps/src/adapter.c"', '#include '+json.dumps(str(ROOT/'lib/PortableApps/src/adapter.c')))
s = s.replace('mock_pixels[240*243]', '*mock_pixels').replace('sizeof(mock_pixels)', 'geometry_size')
s = s.replace('.width=240,.height=240', '.width=geometry_width,.height=geometry_height')
s = s.replace('.stride_bytes=486,', '.stride_bytes=geometry_stride,')
f = out/'fixture.c'; f.write_text(s)
results = []
for sanitized in (False, True):
 name = 'sanitized' if sanitized else 'normal'; binary = out/name
 flags = ['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
 command = [os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function',*flags,
  '-DPORTABLE_RF_NATIVE_CANVAS','-DPORTABLE_NOVA_UI','-DPORTABLE_RASTER_SNAPSHOT','-DPORTABLE_RETAINED_RGB565_HANDOFF',
  '-DFRAME_FIXTURE='+json.dumps(str(f)), '-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),
  str(ROOT/'test/native_apps/native_geometry_test.c'),'-o',str(binary)]
 subprocess.run(command,check=True)
 result = subprocess.run([binary],text=True,capture_output=True,timeout=90,
  env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
 (out/(name+'.log')).write_text(result.stdout+result.stderr);print(result.stdout+result.stderr,end='',flush=True);result.check_returncode()
 results.append({'sanitized':sanitized,'passed':True,'output':result.stdout})
for profile, defines in enumerate(([], ['-DPORTABLE_NOVA_UI','-DPORTABLE_WATCH_COLOR_ONLY'],
 ['-DPORTABLE_NOVA_UI','-DPORTABLE_WATCH_COLOR_ONLY','-DPORTABLE_WATCH_NATIVE_HEIGHT'])):
 for sanitized in (False, True):
  name = f'admission-{profile}-' + ('sanitized' if sanitized else 'normal'); binary = out/name
  flags = ['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
  command = [os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function',*flags,*defines,
   '-DPORTABLE_RETAINED_RGB565_HANDOFF',f'-DTEST_PROFILE={profile}','-DFRAME_FIXTURE='+json.dumps(str(f)),
   '-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),
   str(ROOT/'test/native_apps/native_geometry_admission_test.c'),'-o',str(binary)]
  subprocess.run(command,check=True)
  result = subprocess.run([binary],text=True,capture_output=True,timeout=20,
   env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
  (out/(name+'.log')).write_text(result.stdout+result.stderr);print(result.stdout+result.stderr,end='',flush=True);result.check_returncode()
  results.append({'profile':profile,'sanitized':sanitized,'passed':True,'output':result.stdout})
paths = ['lib/PortableApps/include/PortableDisplayGeometry.h','lib/PortableApps/include/WatchLayout.h',
 'lib/PortableApps/src/adapter.c','lib/PortableApps/src/foreground_adapter_open.inc','lib/PortableApps/src/nova_native.inc',
 'test/native_apps/native_geometry_test.c','test/native_apps/native_geometry_admission_test.c','scripts/test_native_geometry.py']
(out/'results.json').write_text(json.dumps({'results':results,'hardware':False,
 'sha256':{p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in paths}},indent=2)+'\n')

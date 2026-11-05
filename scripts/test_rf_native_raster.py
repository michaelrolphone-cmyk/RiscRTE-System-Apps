#!/usr/bin/env python3
"""Native RF primitive replay: exact pixels, bounded glyph work and custody.

Uses the production adapter and its existing explicit display/touch peripherals.
Reports host CPU cost, not target timing or hardware qualification.
"""
import argparse,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
s=(ROOT/'test/native_apps/portable_handoff_test.c').read_text().replace('#include "../../lib/PortableApps/src/adapter.c"','#include '+json.dumps(str(ROOT/'lib/PortableApps/src/adapter.c')))
s=s.replace('mock_pixels[240*243]','mock_pixels[RF_TEST_H*(RF_TEST_W+3)]').replace('.width=240,.height=240','.width=RF_TEST_W,.height=RF_TEST_H').replace('.stride_bytes=486,','.stride_bytes=(RF_TEST_W+3)*2,')
f=out/'fixture.c';f.write_text(s);runs=[]
for w,h in [(160,160),(240,240),(410,502),(640,240)]:
 for san in (False,True):
  name=f'{w}x{h}'+('-san' if san else '');exe=out/name
  flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
  cmd=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function',*flags,'-DPORTABLE_RF_NATIVE_CANVAS','-DPORTABLE_NOVA_UI','-DPORTABLE_RASTER_SNAPSHOT','-DPORTABLE_RETAINED_RGB565_HANDOFF',f'-DRF_TEST_W={w}',f'-DRF_TEST_H={h}','-DFRAME_FIXTURE='+json.dumps(str(f)),'-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),str(ROOT/'test/native_apps/rf_native_raster_test.c'),'-o',str(exe)]
  subprocess.run(cmd,check=True);r=subprocess.run([str(exe)],text=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'});(out/(name+'.log')).write_text(r.stdout+r.stderr);print(r.stdout+r.stderr,end='',flush=True);r.check_returncode();runs.append({'dimensions':[w,h],'sanitized':san,'command':cmd,'result':r.stdout.strip()})
(out/'evidence.json').write_text(json.dumps({'runs':runs,'hardware_verified':False},indent=2)+'\n')

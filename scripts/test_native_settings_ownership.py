#!/usr/bin/env python3
"""Actual native Settings preferences plus immutable async frame ownership."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
from test_native_settings import ROOT,SIZES,stage_fixture
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
base=stage_fixture(out,'portable_settings_test.c')
# Provider schedules use monotonic time, independent of raster/status capture cadence.
base.write_text(base.read_text().replace('input_script[i].poll==polls','input_script[i].poll==(ticks-native_script_started)/20u'))
s=(ROOT/'test/native_apps/settings_atomic_rgb_test.c').read_text().replace('#include "portable_settings_test.c"',f'#include "{base}"')
s=s[:s.index('int main(int argc,char**argv)')];fixture=out/'atomic-fixture.c';fixture.write_text(s)
env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'};runs=[]
for san in (False,True):
 for rotation in (0,180):
  for snapshot in (False,True):
   name=f'atomic-{int(san)}-{rotation}-{int(snapshot)}';binary=out/name
   cmd=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function',
    '-DPORTABLE_SETTINGS_NATIVE_UI','-DPORTABLE_NATIVE_CANVAS','-DPORTABLE_NOVA_UI','-DPORTABLE_INPUT_NAVIGATION',
    '-DPORTABLE_TOUCH_ROTATION='+str(rotation),f'-DSETTINGS_ATOMIC_FIXTURE="{fixture}"',
    '-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),'-I'+str(a.runtime.resolve()/'sdk/app')]
   if san:cmd+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
   if snapshot:cmd+=['-DPORTABLE_RASTER_SNAPSHOT']
   cmd +=[str(ROOT/'Apps/settings.c'),str(ROOT/'test/native_apps/native_settings_atomic_test.c'),'-o',str(binary)]
   subprocess.run(cmd,check=True,timeout=120)
   for w,h in SIZES:
    for latency in ((0,120,2300) if snapshot else (0,120)):
     for mode in ((tuple(range(8))+(8,9,10) if latency else range(8)) if latency<2300 else (0,8,9,10)):
      result=subprocess.run([str(binary),str(w),str(h),str(mode),str(latency)],env=env,capture_output=True,text=True,timeout=30)
      if result.returncode:raise RuntimeError(f'{name} {w}x{h} case{mode} latency{latency}: '+result.stdout+result.stderr)
      runs.append({'size':[w,h],'sanitized':san,'rotation':rotation,'snapshot':snapshot,'latency_ms':latency,'case':mode,'result':'passed'})
    print(f'{name} {w}x{h}: preference saves/cancels/errors/reentry and immutable pending frames passed',flush=True)
   binary.unlink()
inputs=[ROOT/'test/native_apps/native_settings_atomic_test.c',ROOT/'test/native_apps/settings_atomic_rgb_test.c',Path(__file__),*sorted((ROOT/'lib/PortableApps/src').glob('settings*.inc'))]
receipt={'hardware':'not-tested','runtime_commit':subprocess.check_output(['git','-C',str(a.runtime),'rev-parse','HEAD'],text=True).strip(),'runs':runs,'sha256':{str(f.relative_to(ROOT)):hashlib.sha256(f.read_bytes()).hexdigest() for f in inputs}}
(out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print(f'Native Settings ownership: {len(runs)} cases passed')

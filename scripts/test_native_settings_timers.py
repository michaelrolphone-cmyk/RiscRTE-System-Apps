#!/usr/bin/env python3
"""Capability-sized native Settings timers, real provider/controller/session tests."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
from test_native_settings import ROOT,SIZES,stage_fixture

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--case',type=int,action='append');p.add_argument('--quick',action='store_true');p.add_argument('--snapshot',action='store_true',help='Also exercise pending immutable raster state in the timer contact gate');a=p.parse_args()
 out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);frames=out/'frames';frames.mkdir(exist_ok=True)
 base=stage_fixture(out,'portable_settings_test.c')
 base.write_text(base.read_text().replace('input_script[i].poll==polls','input_script[i].poll==(ticks-native_script_started)/20u'))
 alarm=out/'portable_alarm_test.c';alarm.write_text((ROOT/'test/native_apps/portable_alarm_test.c').read_text().replace('#include "portable_settings_test.c"',f'#include "{base}"'))
 quick=out/'quick_adapter_test.c';quick.write_text((ROOT/'test/native_apps/quick_adapter_test.c').read_text().replace('#include "portable_alarm_test.c"',f'#include "{alarm}"').replace('sizeof(framebuffer)','test_size').replace('int PORTABLE_QUICK_FIXTURE_MAIN(int argc,char **argv)','static int PORTABLE_QUICK_FIXTURE_MAIN(int argc,char **argv)'))
 env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'};runs=[]
 inputs=[ROOT/'Apps/settings.c',ROOT/'test/native_apps/native_settings_timer_test.c',Path(__file__),ROOT/'scripts/test_native_settings.py',*[ROOT/'test/native_apps'/n for n in ('portable_settings_test.c','portable_alarm_test.c','quick_adapter_test.c')],*sorted((ROOT/'lib/PortableApps/src').glob('*.inc')),ROOT/'lib/PortableApps/src/adapter.c',*[ROOT/'lib/PortableApps/src'/n for n in ('quick_actions.c','quick_session.c','quick_radios.c','quick_render.c')],*sorted((ROOT/'lib/PortableApps/include').glob('*.h'))]
 before={str(f.relative_to(ROOT)):hashlib.sha256(f.read_bytes()).hexdigest() for f in inputs}
 for san in ((False,) if a.quick else (False,True)):
  for rotation in ((0,) if a.quick else (0,180)):
   binary=out/f'timers-{int(san)}-{rotation}'
   flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-DPORTABLE_SETTINGS_NATIVE_UI','-DPORTABLE_NATIVE_CANVAS','-DPORTABLE_NOVA_UI','-DPORTABLE_LOW_BATTERY','-DPORTABLE_QUICK_RADIOS','-DPORTABLE_SLEEP_SETTINGS',f'-DPORTABLE_TOUCH_ROTATION={rotation}',f'-DSETTINGS_TIMER_FIXTURE="{quick}"','-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include')]
   if a.snapshot:flags+=['-DPORTABLE_RASTER_SNAPSHOT']
   if san:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
   sources=[ROOT/'Apps/settings.c',ROOT/'test/native_apps/native_settings_timer_test.c',*[ROOT/'lib/PortableApps/src'/n for n in ('quick_actions.c','quick_session.c','quick_radios.c','quick_render.c')]]
   try:
    subprocess.run([os.environ.get('CC','cc'),*flags,*map(str,sources),'-o',str(binary)],check=True,timeout=120)
    for w,h in SIZES:
     for case in a.case or range(11):
      cmd=[str(binary),str(w),str(h),str(case)]
      if not san and rotation==0 and case==0:cmd.append(str(frames))
      r=subprocess.run(cmd,env=env,capture_output=True,text=True,timeout=30)
      if r.returncode:raise RuntimeError(f'{w}x{h} rotation={rotation} sanitized={san} case={case}: '+r.stdout+r.stderr)
      run=json.loads(r.stdout);run.update(rotation=rotation,sanitized=san,snapshot=a.snapshot);runs.append(run)
     print(f'Native timers {w}x{h}, rotation={rotation}, sanitizers={san}: passed',flush=True)
   finally:
    binary.unlink(missing_ok=True)
 after={str(f.relative_to(ROOT)):hashlib.sha256(f.read_bytes()).hexdigest() for f in inputs}
 receipt={'hardware':'not-tested','leak_sanitizer':'not-run: ptrace environment unsupported; ASan and UBSan enabled','runs':runs,'source_sha256':before,'source_changes_during_run':[k for k in before if before[k]!=after[k]],'frame_sha256':{str(f.relative_to(out)):hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(frames.glob('*.ppm'))},'render_evidence':'frames/*.ppm','scope':'Actual Settings controller, raw touch providers, persistence, quick-session reload; gate test uses ordered raw held/released contacts across pending dirty/display/raster states. Async ownership and timezone are separate suites.'}
 (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print(f'Native timers: {len(runs)} passed')
if __name__=='__main__':main()

#!/usr/bin/env python3
"""Real Settings captured-input/presentation regression with provider-only staging."""
import argparse,hashlib,json,os,subprocess,sys
from pathlib import Path
sys.dont_write_bytecode=True
from test_native_settings import ROOT,SIZES,stage_fixture
CASES={0:'sync-previsible-down-up',1:'async-previsible-down-up',2:'explicit-async-drain-real-poll',3:'sync-held-across-completion',4:'async-held-across-completion',5:'sync-fresh-after-completion',6:'async-fresh-after-completion',7:'physical-back-pending',8:'runtime-exit-pending',9:'cancel-after-fresh-entry',10:'close-reenter-fresh',11:'sync-millis32-wrap-stale',12:'sync-zero-raw-timestamp-stale',13:'sync-zero-raw-timestamp-fresh',14:'drain-zero-raw-timestamp-stale',15:'sync-millis32-wrap-fresh',16:'unowned-overlay-no-reset-fresh',17:'sealed-settings-old-overlay-token',18:'same-page-slider-drag',19:'same-page-scroll-drag',20:'unchanged-frame-redraw-tap',21:'async-same-page-slider-drag',22:'async-same-page-scroll-drag',23:'replacement-contact-id-pending',24:'held-contact-cross-page',25:'second-new-contact-before-completion'}
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--quick',action='store_true');p.add_argument('--snapshot',action='store_true');p.add_argument('--case',type=int,choices=CASES,action='append');a=p.parse_args()
 if a.case and 17 in a.case and not a.snapshot:p.error('case 17 requires --snapshot')
 out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
 base=stage_fixture(out,'portable_settings_test.c');s=base.read_text()
 s=s.replace('input_script[i].poll==polls','input_script[i].poll==(uint32_t)(ticks-native_script_started)/20u')
 s=s.replace('ticks += ms;','capture_clock_advance(ms);')
 s=s.replace('.id=polls==replace_at?2:1','.id=capture_contact_id()')
 s=s.replace('.timestamp_ms=ticks,','.timestamp_ms=capture_raw_timestamp(),').replace('next.timestamp_ms=ticks;','next.timestamp_ms=capture_raw_timestamp();')
 s=s.replace('.kind=kind,.id=point.id,.x=point.x,.y=point.y};','.kind=kind,.id=point.id,.x=point.x,.y=point.y};\n  capture_event(kind,touch_state.sequence,capture_raw_timestamp());')
 s=s.replace('*e=touch_events[touch_head];','*e=touch_events[touch_head];deliver_event(e->kind,e->sequence,e->timestamp_ms);')
 s=s.replace('*out=touch_state;return true;','++capture_snapshots;*out=touch_state;return true;')
 base.write_text(s)
 source=(ROOT/'test/native_apps/settings_atomic_rgb_test.c').read_text().replace('#include "portable_settings_test.c"',f'#include "{base}"');source=source[:source.index('int main(int argc,char**argv)')]
 fixture=out/'atomic-fixture.c';fixture.write_text(source)
 inputs=[ROOT/'Apps/settings.c',ROOT/'lib/PortableApps/src/adapter.c',ROOT/'test/native_apps/native_settings_capture_test.c',ROOT/'test/native_apps/settings_atomic_rgb_test.c',ROOT/'test/native_apps/portable_settings_test.c',Path(__file__).resolve(),ROOT/'scripts/test_native_settings.py',*sorted((ROOT/'lib/PortableApps/src').glob('settings*.inc')),*sorted((ROOT/'lib/PortableApps/src').glob('raster_snapshot*.inc')),*sorted((ROOT/'lib/PortableApps/include').glob('*.h'))]
 hashes={str(f.relative_to(ROOT)):hashlib.sha256(f.read_bytes()).hexdigest() for f in inputs};runs=[];builds=[]
 baseline=ROOT/'docs/native-settings/capture-boundary-baseline.json'
 if baseline.exists():(out/'baseline.json').write_bytes(baseline.read_bytes())
 def receipt():
  changed=[str(f.relative_to(ROOT)) for f in inputs if hashes[str(f.relative_to(ROOT))]!=hashlib.sha256(f.read_bytes()).hexdigest()]
  r={'hardware':'not-tested','provider_latency_ms':600,'cases':CASES,'runs':runs,'builds':builds,'source_sha256':hashes,'source_changes_during_run':changed,'fixture_sha256':{f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in (base,fixture)},'production_source_transformed':False,'leak_sanitizer':'not-run: ptrace environment; ASan/UBSan enabled','baseline_sha256':hashlib.sha256(baseline.read_bytes()).hexdigest() if baseline.exists() else None,'runtime_commit':subprocess.check_output(['git','-C',str(a.runtime.resolve()),'rev-parse','HEAD'],text=True).strip(),'runtime_sdk_sha256':{f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted((a.runtime.resolve()/'sdk/app').glob('*.h'))}}
  (out/'receipt.json').write_text(json.dumps(r,indent=2)+'\n')
 env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'}
 try:
  for san in ((False,) if a.quick else (False,True)):
   for rotation in ((0,) if a.quick else (0,180)):
    binary=out/f'capture-{int(san)}-{rotation}';flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-DPORTABLE_SETTINGS_NATIVE_UI','-DPORTABLE_NATIVE_CANVAS','-DPORTABLE_NOVA_UI','-DPORTABLE_INPUT_NAVIGATION',f'-DPORTABLE_TOUCH_ROTATION={rotation}',f'-DSETTINGS_CAPTURE_FIXTURE="{fixture}"','-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include')]
    if a.runtime:flags+=['-I'+str(a.runtime.resolve()/'sdk/app')]
    if a.snapshot:flags+=['-DPORTABLE_RASTER_SNAPSHOT']
    if san:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
    cmd=[os.environ.get('CC','cc'),*flags,str(ROOT/'Apps/settings.c'),str(ROOT/'test/native_apps/native_settings_capture_test.c'),'-o',str(binary)]
    try:
     r=subprocess.run(cmd,capture_output=True,text=True,timeout=120);builds.append({'sanitized':san,'rotation':rotation,'snapshot':a.snapshot,'result':'passed' if not r.returncode else 'failed','stderr':r.stderr});r.check_returncode()
     for w,h in SIZES:
      for case in a.case or [c for c in CASES if c!=17 or a.snapshot]:
       r=subprocess.run([str(binary),str(w),str(h),str(case)],env=env,capture_output=True,text=True,timeout=30)
       name=f'{w}x{h}-r{rotation}-san{int(san)}-case{case}.log'
       if r.returncode or (w,h)==(410,502) and not san and rotation==0:(out/name).write_text(r.stderr+r.stdout)
       run=json.loads(r.stdout) if not r.returncode else {'size':[w,h],'case':case,'result':'failed','log':name}
       run.update(rotation=rotation,sanitized=san,snapshot=a.snapshot);runs.append(run);receipt()
       if r.returncode:raise RuntimeError(f'{name}: '+r.stderr+r.stdout)
      print(f'Capture boundary {w}x{h} rotation={rotation} sanitizers={san}: passed',flush=True)
    finally:binary.unlink(missing_ok=True)
 finally:receipt()
 print(f'Capture boundary: {len(runs)} actual-controller cases passed')
if __name__=='__main__':main()

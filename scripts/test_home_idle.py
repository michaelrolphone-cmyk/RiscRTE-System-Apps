#!/usr/bin/env python3
"""Actual Home timeout enters the desk clock with the existing saved idle timer."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--candidate',type=Path,required=True);p.add_argument('--x4',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output-dir',type=Path,default=ROOT/'build/home-idle-tests');p.add_argument('--normal-only',action='store_true');a=p.parse_args();out=a.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
 receipt=json.loads((a.candidate/'build-evidence.json').read_text());sdk=Path(receipt['paper_transition']['compiled_include_directory']);inc=out/'include';shutil.copytree(sdk,inc,dirs_exist_ok=True)
 # Stage current shared headers over the pinned ABI directory, preserving SDKs.
 for header in (ROOT/'lib/PortableApps/include').glob('*.h'):
  if not header.name.startswith(('Risc','AlarmService')):shutil.copyfile(header,inc/header.name)
 shutil.copytree(ROOT/'lib/PortableApps/time',out/'time',dirs_exist_ok=True)
 omitted={'PORTABLE_CONTEXTS_CLIENT','PORTABLE_CONTEXTS_CLOCK_RF_ONLY','PORTABLE_RESIDENT_LOADING','PORTABLE_SLEEP_MANUAL_ONLY','PORTABLE_RESIDENT_POLICY','PORTABLE_RESIDENT_LEGACY_HANDOFF'}
 flags=[f for f in receipt['build_defines'] if f.startswith('-D') and f[2:].split('=')[0] not in omitted]
 flags+=['-DTEST_NATIVE_LANDSCAPE','-DTEST_HOME_DESK_LOCK']
 if '-DPORTABLE_STAGE_LOGS' not in flags:flags.append('-DPORTABLE_STAGE_LOGS')
 sources=[ROOT/'Apps/paper_clock.c',ROOT/'test/native_apps/home_idle_adapter.c',ROOT/'test/native_apps/home_idle_test.c']
 sources += [ROOT/'lib/PortableApps/src'/n for n in ['desk_clock_faces.c','PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c','quick_actions.c','quick_render.c','quick_session.c','quick_radios.c']]
 sleep=a.x4/'minimal/apps/portable_sleep.c';assert hashlib.sha256(sleep.read_bytes()).hexdigest()==receipt['local_sleep_source_sha256'];sources.append(sleep);sources.append(a.x4/'minimal/apps/portable_idle_sleep.c')
 production=[*ROOT.glob('Apps/*.inc'),ROOT/'Apps/paper_clock.c']+[p for directory in ['lib/PortableApps','lib/NativeApps/include'] for p in (ROOT/directory).rglob('*') if p.is_file()]
 source_inputs={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(set(production))}
 runs=[]
 for name in ('RiscRuntimeV1.h','RiscResidentShellV1.h','RiscFailureEvidenceV1.h'):
  shutil.copyfile(a.runtime/'sdk/app'/name,inc/name)
 sdk_inputs={str(p.relative_to(inc)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(inc.rglob('*')) if p.is_file()}
 extra_inputs={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for directory in (out/'time',a.runtime/'sdk/driver',a.x4/'minimal/drivers/x4pro_power') for p in sorted(directory.rglob('*')) if p.is_file()}
 for san in ([False] if a.normal_only else [False,True]):
  binary=out/('sanitized' if san else 'normal');extra=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
  subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*extra,*flags,'-I'+str(inc),'-I'+str(ROOT/'lib/NativeApps/include'),'-I'+str(a.runtime/'sdk/driver'),'-I'+str(a.x4/'minimal/drivers/x4pro_power'),*map(str,sources),'-o',str(binary)],check=True)
  for seconds,kind in [(5,'terminal'),(60,'terminal'),(180,'terminal'),(60,'refused'),(60,'retained'),(None,'manual')]:
   for direction in ['0','1']:
    directory=out/f'{int(san)}-{seconds}-{kind}-{direction}';directory.mkdir(exist_ok=True);state=directory/'state.bin';state.unlink(missing_ok=True)
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1',CLOCK_EPOCH='1791369720',DESK_DIRECTION=direction,LOCK_CAPTURE=str(directory))
    for name in ('REFERENCE_DRAWER','HOME_IDLE_SECONDS','LOCK_REFUSE','LOCK_RETAIN'):env.pop(name,None)
    if seconds is not None:env['HOME_IDLE_SECONDS']=str(seconds)
    if kind=='refused':env['LOCK_REFUSE']='1'
    if kind=='retained':env['LOCK_RETAIN']='1'
    result=subprocess.run([binary,'manual',state],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=30,env=env)
    (directory/'run.log').write_text(result.stdout)
    assert result.returncode==0,result.stdout
    print(result.stdout.strip(),flush=True);runs.append({'sanitized':san,'seconds':seconds,'kind':kind,'direction':direction,'result':result.stdout.strip()})
    if kind=='terminal' and seconds==60:
     timer_env=dict(env);timer_env.pop('HOME_IDLE_SECONDS',None)
     for minute in range(1,4):
      result=subprocess.run([binary,'terminal',state],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=30,env=timer_env)
      (directory/f'timer-{minute}.log').write_text(result.stdout)
      assert result.returncode==0,result.stdout
      print(result.stdout.strip(),flush=True);runs.append({'sanitized':san,'kind':'retained-minute','minute':minute,'direction':direction,'result':result.stdout.strip()})
 assert all(hashlib.sha256((ROOT/p).read_bytes()).hexdigest()==digest for p,digest in source_inputs.items()),'Production input changed during matrix'
 assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==digest for p,digest in extra_inputs.items()),'Included SDK input changed during matrix'
 (out/'evidence.json').write_text(json.dumps({'runs':runs,'build_defines':flags,'runtime_revision':subprocess.check_output(['git','-C',str(a.runtime),'rev-parse','HEAD'],text=True).strip(),'production_source_sha256':source_inputs,'staged_sdk_sha256':sdk_inputs,'additional_include_sha256':extra_inputs,'process_cases':len(runs),'runtime_mocked':True,'hardware_tested':False,'boundary':'Actual sparse Home, adapter, persisted idle timer, desk renderer and product sleep hook; strict synthetic native Runtime/power callbacks. No hardware claim.','excluded_unrelated_profiles':sorted(omitted),'source_sha256':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [*sources,ROOT/'test/native_apps/sparse_clock_startup_test.c',ROOT/'test/native_apps/paper_clock_test.c',ROOT/'test/native_apps/shared_quick_reference.h',Path(__file__).resolve()]}},indent=2)+'\n')
if __name__=='__main__':main()

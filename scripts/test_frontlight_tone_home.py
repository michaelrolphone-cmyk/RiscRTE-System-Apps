#!/usr/bin/env python3
"""Selected sparse Home present-tone startup, GPIO wake, and sparse-minute isolation."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--candidate',type=Path,required=True);p.add_argument('--x4',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output-dir',type=Path,default=ROOT/'build/shared-quick-home');p.add_argument('--normal-only',action='store_true');a=p.parse_args();out=a.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
 receipt=json.loads((a.candidate/'build-evidence.json').read_text());sdk=Path(receipt['paper_transition']['compiled_include_directory']);inc=out/'include';shutil.copytree(sdk,inc,dirs_exist_ok=True)
 # Stage current shared headers over the pinned ABI directory, preserving SDKs.
 for header in (ROOT/'lib/PortableApps/include').glob('*.h'):
  if not header.name.startswith(('Risc','AlarmService')):shutil.copyfile(header,inc/header.name)
 shutil.copytree(ROOT/'lib/PortableApps/time',out/'time',dirs_exist_ok=True)
 omitted={'PORTABLE_CONTEXTS_CLIENT','PORTABLE_CONTEXTS_CLOCK_RF_ONLY','PORTABLE_RESIDENT_POLICY','PORTABLE_RESIDENT_LEGACY_HANDOFF','PORTABLE_X4_IDLE_POLICY','PORTABLE_LOW_BATTERY'}
 flags=[f for f in receipt['build_defines'] if f.startswith('-D') and f[2:].split('=')[0] not in omitted]
 flags+=['-DTEST_NATIVE_LANDSCAPE','-DTEST_HOME_DESK_LOCK']
 sources=[ROOT/'Apps/paper_clock.c',ROOT/'test/native_apps/shared_quick_adapter.c',ROOT/'test/native_apps/frontlight_tone_home_test.c']
 sources += [ROOT/'lib/PortableApps/src'/n for n in ['desk_clock_faces.c','PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c','quick_actions.c','quick_render.c','quick_session.c','quick_radios.c']]
 sleep=a.x4/'minimal/apps/portable_sleep.c';assert hashlib.sha256(sleep.read_bytes()).hexdigest()==receipt['local_sleep_source_sha256'];sources.append(sleep)
 production=[*ROOT.glob('Apps/*.inc'),ROOT/'Apps/paper_clock.c']+[p for directory in ['lib/PortableApps','lib/NativeApps/include'] for p in (ROOT/directory).rglob('*') if p.is_file()]
 source_inputs={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(set(production))}
 sdk_inputs={str(p.relative_to(inc)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(inc.rglob('*')) if p.is_file()}
 runs=[]
 for san in ([False] if a.normal_only else [False,True]):
  binary=out/('sanitized' if san else 'normal');extra=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
  subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*extra,*flags,'-I'+str(inc),'-I'+str(ROOT/'lib/NativeApps/include'),'-I'+str(a.runtime/'sdk/driver'),'-I'+str(a.x4/'minimal/drivers/x4pro_power'),*map(str,sources),'-o',str(binary)],check=True)
  for case in ['cold','gpio','cold-get-failed','cold-set-failed','gpio-get-failed','gpio-set-failed','minute']:
   directory=out/f'{int(san)}-{case}';directory.mkdir(exist_ok=True);state=directory/'state.bin';state.unlink(missing_ok=True)
   env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1',CLOCK_EPOCH='1791369720');env.pop('REFERENCE_DRAWER',None)
   r=subprocess.run([binary,case,state],check=True,text=True,stdout=subprocess.PIPE,timeout=30,env=env);print(r.stdout.strip(),flush=True);runs.append({'sanitized':san,'case':case,'result':r.stdout.strip()})
 assert all(hashlib.sha256((ROOT/p).read_bytes()).hexdigest()==digest for p,digest in source_inputs.items()),'Production input changed during matrix'
 (out/'evidence.json').write_text(json.dumps({'runs':runs,'build_defines':flags,'runtime_revision':subprocess.check_output(['git','-C',str(a.runtime),'rev-parse','HEAD'],text=True).strip(),'production_source_sha256':source_inputs,'staged_sdk_sha256':sdk_inputs,'process_cases':len(runs),'runtime_mocked':True,'hardware_tested':False,'boundary':'Actual sparse Home, adapter, desk renderer and pinned product sleep hook; strict synthetic native Runtime/power callbacks. Real Runtime/ELF routing is qualified separately.','excluded_unrelated_profiles':sorted(omitted),'source_sha256':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [*sources,ROOT/'test/native_apps/sparse_clock_startup_test.c',ROOT/'test/native_apps/paper_clock_test.c',ROOT/'test/native_apps/shared_quick_home_test.c',ROOT/'test/native_apps/shared_quick_reference.h',Path(__file__).resolve()]}},indent=2)+'\n')
if __name__=='__main__':main()

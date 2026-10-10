#!/usr/bin/env python3
"""Selected production Settings lists with exact target defines/SDK.
Only providers are simulated. Frames are actual MONO1 rasters, not mock UI.
"""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
import test_native_time_settings as native
import test_touch_scroll_settings as timezone
ROOT=Path(__file__).resolve().parents[1]
CASES='drag select busy-hit stop-tap reverse bounds back home horizontal replaced cancelled footer-drag modal alarm queued-up nested partial supersede reentry render-retained static-grid cancel save'.split()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--target-dir',type=Path,required=True)
 p.add_argument('--output-dir',type=Path,default=ROOT/'build/settings-list-tests')
 p.add_argument('--normal-only',action='store_true')
 p.add_argument('--regressions',action='store_true',help='Also run existing timezone, native RTC and fixed-editor fixtures with selected target policy')
 p.add_argument('--case',action='append',choices=CASES)
 a=p.parse_args();target=a.target_dir.resolve();out=a.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
 record=json.loads((target/'settings-build-record.json').read_text())
 resident=record.get('resident_shell',{}).get('role')=='foreground'
 assert record['version']==json.loads((target/'settings.json').read_text())['version']
 assert record['touch_scrolling']['version']==2
 include=Path(record['idle_policy']['compiled_include_directory']) if record.get('idle_policy') else target/('performance-sdk/include' if record.get('performance_trace') else 'native-time-sdk/include')
 for name,digest in record['source_sha256'].items():
  assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==digest, 'Rebuild target after editing '+name
 expected=dict(record['native_time_sdk_headers'])
 if record.get('performance_trace'):
  expected.update(record['performance_trace']['sdk_headers'])
  expected.update(record['performance_trace'].get('display_metrics',{}).get('sha256',{}))
 expected.update({name:digest for name,digest in record['tagged_alarm_sdk']['sha256'].items() if name!='LICENSE'})
 expected.update(record.get('idle_policy',{}).get('sdk_sha256',{}))
 expected.update(record.get('resident_shell',{}).get('sdk_sha256',{}))
 for name,digest in expected.items():assert hashlib.sha256((include/name).read_bytes()).hexdigest()==digest,name
 fixture_defines=set('PORTABLE_SETTINGS_APP PORTABLE_SETTINGS_NATIVE_TIME PORTABLE_SETTINGS_TIME_ZONE PORTABLE_SETTINGS_X4_DESK_CLOCK PORTABLE_SLEEP_SETTINGS PORTABLE_INPUT_NAVIGATION PORTABLE_QUICK_ACTIONS PORTABLE_QUICK_RADIOS PORTABLE_ALARM_CLIENT PORTABLE_ALARM_SETTINGS'.split())
 flags=[f for f in record['build_defines'] if f[2:] not in fixture_defines]+['-DTEST_NATIVE_SETTINGS_ALARMS' if resident else '-DTEST_NATIVE_SETTINGS_QUICK']
 sources=[ROOT/'Apps/settings_native_entry.c',ROOT/'test/native_apps/touch_scroll_settings_lists_test.c',*[ROOT/'lib/PortableApps/src'/n for n in native.HELPERS+([] if resident else ['quick_actions.c','quick_render.c','quick_session.c','quick_radios.c'])]]
 receipt={'target_sha256':record['sha256'],'runtime_ref':record['native_time_runtime_commit'],'hardware':'not run','sdk_sha256':expected,'runs':{},'native_regression_policy':'RTC fixture omits only the root return-app define; production list tests use every target define. Legacy paging timelines and async-touch before any completed image are covered by the current list identity fixtures instead.'}
 for short in (False,True):
  for flipped in (False,True):
   for san in (False,) if a.normal_only else (False,True):
    label=('short' if short else 'paper')+('-flip' if flipped else '')+('-san' if san else '')
    extra=(['-DTEST_NATIVE_SETTINGS_SHORT'] if short else [])+(['-DTEST_SETTINGS_LIST_FLIPPED'] if flipped else [])
    if san:extra+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
    binary=out/label
    linked=sources.copy()
    if record.get('idle_policy'):
     helper=out/(label+'-idle.o')
     subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g',*record['build_defines'],*extra,'-I'+str(include),'-c',record['idle_policy']['source'],'-o',helper],check=True)
     linked+=[helper]
    common=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function',*flags,*extra,'-I'+str(include),'-I'+str(ROOT/'lib/NativeApps/include')]
    subprocess.run([*common,*linked,'-Wl,--wrap=free','-o',binary],check=True)
    results=[]
    for which in ('root','fields'):
     for case in a.case or CASES:
      if resident and case in ('modal','alarm'):continue  # Host rendering is qualified by resident policy tests.
      if (which=='root' and case=='save') or (which=='fields' and case in ('render-retained','static-grid')):continue
      frames=out/(label+'-frames')/(which+'-'+case);frames.mkdir(parents=True,exist_ok=True)
      result=subprocess.run([binary,which,case,*([frames] if not san else [])],env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'),text=True,stdout=subprocess.PIPE,check=True,timeout=20)
      data=json.loads(result.stdout);results.append(data);print(label,which,case,data,flush=True)
    receipt['runs'][label]=results
    if a.regressions and not flipped:
     cases=[name for name in native.CASES+native.ALARM_CASES+native.QUICK_CASES if name not in ('async-timezone-next','sync-timezone-next','async-touch')]
     for fixture,checks in [('touch_scroll_settings_test.c',timezone.CASES),('portable_native_time_settings_test.c',cases)]:
      exe=out/(label+'-'+fixture.removesuffix('.c'))
      inputs=[ROOT/'test/native_apps'/fixture if str(source).endswith('/touch_scroll_settings_lists_test.c') else source for source in linked]
      regression_common=[flag for flag in common if not flag.startswith('-DPORTABLE_RETURN_APP=')] if fixture=='portable_native_time_settings_test.c' else common
      subprocess.run([*regression_common,*inputs,'-Wl,--wrap=free','-o',exe],check=True)
      passed=[]
      for check in checks:
       result=subprocess.run([exe,check],env=native.environment(check),text=True,stdout=subprocess.PIPE,check=True,timeout=20)
       passed.append(check)
      receipt['runs'][label+'-'+fixture]=passed;print(label,fixture,len(passed),'regression cases passed',flush=True)
     if record.get('idle_policy'):
      exe=out/(label+'-idle');fixture=ROOT/'test/native_apps/x4_idle_settings_test.c'
      idle_inputs=[fixture if str(source).endswith('/touch_scroll_settings_lists_test.c') else source for source in sources]
      idle_common=[flag for flag in common if not flag.startswith('-DPORTABLE_RETURN_APP=')]
      idle_common+=['-DTEST_X4_IDLE_SETTINGS','-DPORTABLE_RADIO_CONTINUOUS_CAPTURE','-DPORTABLE_AUDIO_CONTINUOUS_CAPTURE']
      subprocess.run([*idle_common,*idle_inputs,'-Wl,--wrap=free','-o',exe],check=True)
      for result in (0,1,-2):
       subprocess.run([exe,str(result)],check=True,env=native.environment('value-edit'),stdout=subprocess.PIPE,timeout=20)
      receipt['runs'][label+'-idle']=[0,1,-2]
      print(label,'automatic Light refusal/resume/retained and admission gates passed',flush=True)
 receipt['process_cases']=sum(map(len,receipt['runs'].values()))
 receipt['source_commit']=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
 receipt['source_sha256']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [*ROOT.glob('lib/PortableApps/src/settings*.inc'),ROOT/'test/native_apps/touch_scroll_settings_lists_test.c',ROOT/'test/native_apps/portable_native_time_settings_test.c',ROOT/'test/native_apps/x4_idle_settings_test.c',ROOT/'lib/PortableApps/src/nova.inc',ROOT/'lib/PortableApps/src/adapter.c',Path(__file__).resolve()]}
 (out/'evidence.json').write_text(json.dumps(receipt,indent=2)+'\n')
if __name__=='__main__':main()

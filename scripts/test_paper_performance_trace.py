#!/usr/bin/env python3
"""Production controller/adapter trace order, compatibility and retention."""
import argparse,json,os,shutil,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--runtime-sdk',type=Path,required=True)
p.add_argument('--display-sdk',type=Path)
p.add_argument('--output-dir',type=Path,default=ROOT/'build/performance')
a=p.parse_args();a.output_dir.mkdir(parents=True,exist_ok=True)
cases=['launch-feedback','launch-same','launch-held','launch-retry','launch-replace','launch-cancel','launch-drag','launch-multi','launch-held-entry','launch-failed','launch-status-false','launch-submit-false','launch-timeout','launch-quick','launch-alarm']
results=[]
with tempfile.TemporaryDirectory(prefix='paper-performance-') as directory:
 stage=Path(directory);include=stage/'include';shutil.copytree(ROOT/'lib/PortableApps/include',include);shutil.copytree(ROOT/'lib/PortableApps/time',stage/'time')
 for name in ['RiscRuntimeV1.h','RiscRealtimeV1.h','RiscPerformanceV1.h']:shutil.copyfile(a.runtime_sdk/name,include/name)
 for sanitizer in (False,True):
  binary=a.output_dir/('trace-sanitized' if sanitizer else 'trace')
  flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitizer else []
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,
   '-DPORTABLE_PERFORMANCE_TRACE','-DTEST_APP_COMPATIBLE=true','-DPORTABLE_NATIVE_TIME_TOOLBAR','-DPORTABLE_NATIVE_CUSTODY_FENCE','-DTEST_NATIVE_TOOLBAR_QUICK','-DPORTABLE_INPUT_NAVIGATION',
   '-I'+str(include),'-I'+str(ROOT/'lib/NativeApps/include'),str(ROOT/'test/native_apps/paper_performance_trace_test.c'),str(ROOT/'test/native_apps/paper_present_controller.c'),
   *[str(ROOT/'lib/PortableApps/src'/name) for name in ['quick_actions.c','quick_render.c','quick_session.c']],'-Wl,--wrap=free','-o',str(binary)],check=True)
  for mode in ('enabled','disabled','old'):
   for case in cases:
    r=subprocess.run([str(binary),case,mode],env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'),text=True,capture_output=True)
    if r.returncode:raise AssertionError((case,mode,sanitizer,r.stdout,r.stderr))
    results.append(dict(case=case,mode=mode,sanitized=sanitizer,result=json.loads(r.stdout)))
  print(f'Production trace: {len(cases)*3} {"ASan/UBSan" if sanitizer else "normal"} cases passed',flush=True)
  # The real Settings Next footer and repeated/held touch workload.
  import test_native_time_settings as settings_suite
  settings_binary=a.output_dir/('settings-trace-sanitized' if sanitizer else 'settings-trace')
  helpers=['PortableSetTime.c','PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c']
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,
   '-DPORTABLE_PERFORMANCE_TRACE','-DPORTABLE_NATIVE_CUSTODY_FENCE','-I'+str(include),'-I'+str(ROOT/'lib/NativeApps/include'),
   str(ROOT/'Apps/settings_native_entry.c'),str(ROOT/'test/native_apps/settings_performance_trace_test.c'),
   *[str(ROOT/'lib/PortableApps/src'/name) for name in helpers],'-Wl,--wrap=free','-o',str(settings_binary)],check=True)
  settings_cases=['async-timezone-next','sync-timezone-next','async-edit','async-touch','async-exit','async-retained','native-context','metadata-read-context']
  for case in settings_cases:
   r=subprocess.run([str(settings_binary),case],env=settings_suite.environment(case),text=True,capture_output=True)
   if r.returncode:raise AssertionError((case,sanitizer,r.stdout,r.stderr))
   results.append(dict(app='settings',case=case,sanitized=sanitizer,result=json.loads(r.stdout)))
  print(f'Settings trace: {len(settings_cases)} {"ASan/UBSan" if sanitizer else "normal"} cases passed',flush=True)
  if a.display_sdk:
   for name in ['RiscDisplayOutputV1.h','RiscDisplayOutputPowerV1.h','RiscDisplayOutputMetricsV1.h']:shutil.copyfile(a.display_sdk/name,include/name)
   metrics_binary=a.output_dir/('metrics-sanitized' if sanitizer else 'metrics')
   subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,
    '-DPORTABLE_PERFORMANCE_TRACE','-DPORTABLE_PERFORMANCE_DISPLAY_METRICS','-DPORTABLE_NATIVE_TIME_TOOLBAR','-DPORTABLE_NATIVE_CUSTODY_FENCE',
    '-I'+str(include),'-I'+str(ROOT/'lib/NativeApps/include'),str(ROOT/'test/native_apps/display_performance_metrics_test.c'),'-Wl,--wrap=free','-o',str(metrics_binary)],check=True)
   subprocess.run([str(metrics_binary)],env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'),check=True)
(a.output_dir/'trace-evidence.json').write_text(json.dumps({'hardware':'not run','cases':results},indent=2)+'\n')

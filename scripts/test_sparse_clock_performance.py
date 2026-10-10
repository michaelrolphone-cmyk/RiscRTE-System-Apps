#!/usr/bin/env python3
"""Production Clock boot-stage traces against deterministic capability doubles."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--clock-build',type=Path,required=True)
p.add_argument('--x4',type=Path,required=True)
p.add_argument('--output-dir',type=Path,default=ROOT/'build/performance-clock')
a=p.parse_args();a.output_dir.mkdir(parents=True,exist_ok=True)
include=a.clock_build/'performance-sdk/include'
record=json.loads((a.clock_build/'build-evidence.json').read_text())
assert record['performance_trace']['enabled']
for name,digest in record['performance_trace']['sdk_headers'].items():assert hashlib.sha256((include/name).read_bytes()).hexdigest()==digest,name
names=['adapter.c','desk_clock_faces.c','PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c','quick_actions.c','quick_render.c','quick_session.c','quick_radios.c']
flags='TEST_NATIVE_LANDSCAPE PORTABLE_DISPLAY_ROTATION=90 PORTABLE_APP_OWNS_TOUCH_CHROME PORTABLE_RTC_WALL_TIME PORTABLE_ALARM_CLIENT PORTABLE_INPUT_NAVIGATION PORTABLE_APP_SLEEP_LOCAL PORTABLE_CROWN_SLEEP_LOCAL PORTABLE_SLEEP_MANUAL_ONLY PORTABLE_DESK_CLOCK PORTABLE_DESK_CLOCK_SPARSE_START ALARM_SERVICE_TAGGED_V2 PORTABLE_QUICK_ACTIONS PORTABLE_QUICK_RADIOS PORTABLE_HOME_POINTS_NATIVE_UTC ALARM_NATIVE_UTC PORTABLE_PERFORMANCE_TRACE PORTABLE_PERFORMANCE_DISPLAY_METRICS'.split()
cases=['cold','gpio','native-context','native-release','key-retained','promotion-retained','cold-acquire','cold-submit','cold-wait','raw-swipe-right','raw-held-swipe']
results=[]
for sanitizer in (False,True):
 binary=a.output_dir/('clock-sanitized' if sanitizer else 'clock')
 san=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitizer else []
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*san,*['-D'+f for f in flags],
  '-I'+str(include),'-I'+str(ROOT/'lib/NativeApps/include'),'-I'+str(a.x4/'minimal/drivers/x4pro_power'),
  str(ROOT/'Apps/paper_clock.c'),*[str(ROOT/'lib/PortableApps/src'/name) for name in names],str(a.x4/'minimal/apps/portable_sleep.c'),str(ROOT/'test/native_apps/sparse_clock_performance_trace_test.c'),'-o',str(binary)],check=True)
 for case in cases:
  state=a.output_dir/'state';state.unlink(missing_ok=True)
  run=subprocess.run([str(binary),case,str(state)],env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'),capture_output=True,text=True)
  if run.returncode:raise AssertionError((case,sanitizer,run.stdout,run.stderr))
  results.append(dict(case=case,sanitized=sanitizer,output=run.stdout,trace=run.stderr))
 print(f'Sparse Clock tracing: {len(cases)} {"ASan/UBSan" if sanitizer else "normal"} cases passed',flush=True)
(a.output_dir/'evidence.json').write_text(json.dumps(dict(hardware='not run',runs=results),indent=2)+'\n')

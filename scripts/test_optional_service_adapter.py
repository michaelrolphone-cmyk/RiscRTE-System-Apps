#!/usr/bin/env python3
"""Qualified optional absence, actual adapter startup/poll/fini; no device claim."""
import json,os,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
base=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-DPORTABLE_CONTEXTS_CLIENT','-DPORTABLE_BLE_BROADCAST','-DPORTABLE_QUICK_RADIOS','-DPORTABLE_LOW_BATTERY','-DPORTABLE_SLEEP_SETTINGS','-DPORTABLE_NOVA_UI','-DPORTABLE_TOUCH_ROTATION=0','-I'+str(root/'lib/PortableApps/include'),'-I'+str(root/'lib/NativeApps/include')]
sources=[root/'Apps/settings.c',root/'test/native_apps/optional_service_adapter_test.c',*[root/'lib/PortableApps/src'/n for n in ('quick_actions.c','quick_session.c','quick_radios.c','quick_render.c')]]
cases=[(mask,0,0) for mask in range(8)]+[(0,service,fault) for service in (1,2,3) for fault in (2,3,4,5,6,7,9,10)]+[(0,1,fault) for fault in (11,12,13,14,15,16,18,19,20,21)]+[(0,service,fault) for service in (1,2) for fault in (22,23,24,25,27)]+[(0,1,fault) for fault in range(30,39)]+[(0,service,fault) for service in (1,2) for fault in (40,41)]+[(0,1,fault) for fault in range(50,54)]
runs=[];leak_limited=False
profiles=[('240',[]),('410-async',['-DPORTABLE_WATCH_NATIVE_HEIGHT','-DSETTINGS_FIXTURE_WIDTH=410','-DSETTINGS_FIXTURE_HEIGHT=502','-DPORTABLE_ASYNC_RGB565'])]
with tempfile.TemporaryDirectory(prefix='optional-service-adapter-') as tmp:
 for profile,definitions in profiles:
  for sanitized in (False,True):
   flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
   exe=Path(tmp)/(profile+str(sanitized))
   subprocess.run([os.environ.get('CC','cc'),*base,*definitions,*flags,*map(str,sources),'-o',str(exe)],check=True)
   for case in cases:
    env=os.environ.copy()
    if leak_limited:env['ASAN_OPTIONS']='detect_leaks=0'
    result=subprocess.run([str(exe),*map(str,case)],capture_output=True,text=True,timeout=30,env=env)
    if result.returncode and 'LeakSanitizer does not work under ptrace' in result.stderr:
     leak_limited=True;env['ASAN_OPTIONS']='detect_leaks=0'
     result=subprocess.run([str(exe),*map(str,case)],capture_output=True,text=True,timeout=30,env=env)
    if result.returncode:raise RuntimeError(f'{profile} sanitizer={sanitized} case={case}\n'+result.stdout+result.stderr)
    runs.append({'profile':profile,'sanitized':sanitized,'case':case})
    print(result.stdout.strip(),flush=True)
report={'runs':len(runs),'cases':runs,'asan_ubsan':True,'leak_sanitizer_ptrace_limited':leak_limited,'hardware_qualified':False}
out=root/'build/optional-services';out.mkdir(parents=True,exist_ok=True)
(out/'result.json').write_text(json.dumps(report,indent=2)+'\n')
print(f'Optional service adapter: {len(runs)} normal + ASan/UBSan cases PASS')

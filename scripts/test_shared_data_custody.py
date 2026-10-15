#!/usr/bin/env python3
"""Test the production shared-data custody wrapper on MONO1 and RGB providers.

These host profiles prove wrapper behavior, not deployed product source closure
or native provider/hardware behavior. SDK and Utilities roots must be explicit.
"""
import argparse, hashlib, json, os, shutil, subprocess, tempfile
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--source-root',type=Path,default=Path(__file__).resolve().parents[1])
p.add_argument('--test-source',type=Path)
p.add_argument('--sdk-include',type=Path,required=True)
p.add_argument('--app-data-sdk',type=Path,required=True)
p.add_argument('--utilities-root',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--baseline',action='store_true',help='Expect unchanged first-poll failure instead of repair')
p.add_argument('--callback-audit',action='store_true',help='Also check retained-success capture/step callbacks')
a=p.parse_args();r=a.source_root.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
test=(a.test_source or r/'test/native_apps/shared_data_custody_test.c').resolve()
cases=('baseline-first-poll' if a.baseline else 'first-poll clean-2 clean-3 clean-4 dirty-1 dirty-2 dirty-3 dirty-4 dirty-5 bad-version bad-namespace-1 bad-namespace-5 revoked-identity revoked-health missing-health failed-release').split()
if a.callback_audit:cases+=['capture-raster-retained','capture-input-retained','capture-present-retained','step-caller-retained','fingerprint-caller-retained','fingerprint-open-retained','fingerprint-generic-open-retained','fingerprint-checkpoint-retained']
profiles={'paper-async':[], 'paper-sync':['-DAUDIT_SYNCHRONOUS_PAPER'],
          'rgb240':['-DAUDIT_RGB_WIDTH=240','-DAUDIT_RGB_HEIGHT=240'],
          'rgb410':['-DAUDIT_RGB_WIDTH=410','-DAUDIT_RGB_HEIGHT=502']}
for key in ['rgb240','rgb410']:
 profiles[key]+=['-DPORTABLE_NOVA_UI','-DPORTABLE_WATCH_NATIVE_HEIGHT','-DPORTABLE_WATCH_COLOR_ONLY','-DPORTABLE_ASYNC_RGB565']
tracked=[r/'lib/PortableApps/src/adapter.c',r/'lib/PortableApps/src/native_custody_adapter.inc',r/'lib/PortableApps/src/contexts_adapter.inc',r/'lib/PortableApps/src/contexts_policy.inc',r/'test/native_apps/context_x4_adapter_test.c',r/'test/native_apps/portable_native_toolbar_test.c',*sorted((r/'lib/PortableApps/include').glob('PortableContext*.h')),test]
receipt={'source_root':str(r),'source_commit':subprocess.check_output(['git','-C',str(r),'rev-parse','HEAD'],text=True).strip(),
 'compiler':subprocess.check_output([os.environ.get('CC','cc'),'--version'],text=True).splitlines()[0],
 'hashes':{str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in tracked},'commands':[],'results':[]}
with tempfile.TemporaryDirectory(prefix='shared-data-custody-') as td:
 t=Path(td);inc=t/'include';shutil.copytree(r/'lib/PortableApps/include',inc);shutil.copytree(r/'lib/PortableApps/time',t/'time')
 for n in ['RiscRuntimeV1.h','RiscRealtimeV1.h','AlarmServiceV1.h','AlarmServiceV2.h']:
  src=a.sdk_include/n;shutil.copyfile(src,inc/n);receipt['hashes'][str(src)]=hashlib.sha256(src.read_bytes()).hexdigest()
 src=a.app_data_sdk/'RiscAppDataV1.h';shutil.copyfile(src,inc/src.name);receipt['hashes'][str(src)]=hashlib.sha256(src.read_bytes()).hexdigest()
 # Rename only the existing fixture's main. Production source is never rewritten.
 fixture=(r/'test/native_apps/context_x4_adapter_test.c').read_text()
 assert fixture.count('int main(')==1
 fixture=fixture.replace('int main(','int existing_context_main(')
 fixture=fixture.replace('#include "portable_native_toolbar_test.c"','#include "'+str(r/'test/native_apps/portable_native_toolbar_test.c')+'"')
 (t/'context-custody-fixture.inc').write_text(fixture)
 flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-DPORTABLE_CONTEXTS_CLIENT','-DPORTABLE_NATIVE_TIME_TOOLBAR','-DPORTABLE_NATIVE_CUSTODY_FENCE','-DALARM_SERVICE_TAGGED_V2','-DTEST_NATIVE_TOOLBAR_QUICK','-DPORTABLE_APP_SLEEP_LOCAL','-DPORTABLE_X4_IDLE_POLICY','-DPORTABLE_PAPER_PREFERENCES',
  '-I'+str(t),'-I'+str(inc),'-I'+str(r/'lib/NativeApps/include'),'-I'+str(r/'test/native_apps'),'-I'+str(a.utilities_root/'Apps'),'-I'+str(a.utilities_root/'lib/Alarm/include')]
 failed=False
 for profile,defs in profiles.items():
  for san in [False,True]:
   mode='sanitized' if san else 'normal';exe=out/(profile+'-'+mode)
   extra=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
   cmd=[os.environ.get('CC','cc'),*flags,*defs,*extra,str(test),*[str(r/'lib/PortableApps/src'/n) for n in ['quick_actions.c','quick_session.c','quick_render.c']],'-Wl,--wrap=free','-o',str(exe)]
   receipt['commands'].append(cmd);subprocess.run(cmd,check=True)
   for case in cases:
    run=subprocess.run([exe,case],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=20,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'))
    log=out/(profile+'-'+mode+'-'+case+'.log');log.write_text(run.stdout)
    result={'profile':profile,'mode':mode,'case':case,'exit':run.returncode,'log':str(log)};receipt['results'].append(result)
    print(profile,mode,case,'exit='+str(run.returncode),flush=True)
    if run.returncode:failed=True;print(run.stdout[-2500:],flush=True)
 (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
 raise SystemExit(1 if failed else 0)

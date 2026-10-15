#!/usr/bin/env python3
"""Exercise the shipped generic adapter's first-frame RGB brightness policy."""
import argparse,hashlib,json,os,shutil,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--source-root',type=Path,default=Path(__file__).resolve().parents[1])
p.add_argument('--sdk-include',type=Path,required=True)
p.add_argument('--app-data-sdk',type=Path,required=True)
p.add_argument('--utilities-root',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--baseline',action='store_true')
a=p.parse_args();r=a.source_root.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
test=Path(__file__).resolve().parents[1]/'test/native_apps/shared_initial_brightness_test.c'
profiles={'rgb240':['-DBRIGHTNESS_RGB_WIDTH=240','-DBRIGHTNESS_RGB_HEIGHT=240'],
 'rgb410':['-DBRIGHTNESS_RGB_WIDTH=410','-DBRIGHTNESS_RGB_HEIGHT=502']}
if not a.baseline:profiles['paper']=[]
receipt={'source_commit':subprocess.check_output(['git','-C',r,'rev-parse','HEAD'],text=True).strip(),
 'hashes':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in [r/'lib/PortableApps/src/adapter.c',r/'lib/PortableApps/src/foreground_adapter_open.inc',test]},'commands':[],'results':[],'hardware_tested':False}
with tempfile.TemporaryDirectory(prefix='shared-brightness-') as td:
 t=Path(td);inc=t/'include';shutil.copytree(r/'lib/PortableApps/include',inc);shutil.copytree(r/'lib/PortableApps/time',t/'time')
 for n in ['RiscRuntimeV1.h','RiscRealtimeV1.h','AlarmServiceV1.h','AlarmServiceV2.h']:shutil.copyfile(a.sdk_include/n,inc/n)
 shutil.copyfile(a.app_data_sdk/'RiscAppDataV1.h',inc/'RiscAppDataV1.h')
 flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-DPORTABLE_NATIVE_TIME_TOOLBAR','-DPORTABLE_NATIVE_CUSTODY_FENCE','-DTEST_NATIVE_TOOLBAR_QUICK','-DALARM_SERVICE_TAGGED_V2','-DPORTABLE_PAPER_PREFERENCES',
 '-DTOOLBAR_FIXTURE="'+str(r/'test/native_apps/portable_native_toolbar_test.c')+'"','-I'+str(inc),'-I'+str(r/'lib/NativeApps/include'),'-I'+str(r/'test/native_apps'),'-I'+str(a.utilities_root/'Apps'),'-I'+str(a.utilities_root/'lib/Alarm/include')]
 for profile,defs in profiles.items():
  if profile!='paper':defs+=['-DPORTABLE_NOVA_UI','-DPORTABLE_WATCH_NATIVE_HEIGHT','-DPORTABLE_WATCH_COLOR_ONLY','-DPORTABLE_ASYNC_RGB565']
  cases=['dark-open'] if a.baseline or profile=='paper' else ['dark-open','retained-lit','saved-level','preview-before','torch-before','unsupported','missing-callback','submit-failed','status-failed','completion-failed','superseded','timeout','status-retained','brightness-failed','brightness-retained']
  for san in [False,True]:
   mode='sanitized' if san else 'normal';exe=out/(profile+'-'+mode)
   extra=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
   if a.baseline:extra+=['-DBRIGHTNESS_BASELINE']
   cmd=[os.environ.get('CC','cc'),*flags,*defs,*extra,str(test),*[str(r/'lib/PortableApps/src'/n) for n in ['quick_actions.c','quick_session.c','quick_render.c']],'-Wl,--wrap=free','-o',str(exe)]
   receipt['commands'].append(cmd);subprocess.run(cmd,check=True)
   for timing in ['sync','async']:
    for case in cases:
     run=subprocess.run([exe,timing,case],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=30,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'))
     log=out/(profile+'-'+mode+'-'+timing+'-'+case+'.log');log.write_text(run.stdout)
     receipt['results'].append({'profile':profile,'mode':mode,'timing':timing,'case':case,'exit':run.returncode,'log':log.name})
     print(profile,mode,timing,case,'exit='+str(run.returncode),flush=True)
     if run.returncode:print(run.stdout);raise SystemExit(run.returncode)
 if not a.baseline:
  for san in [False,True]:
   mode='sanitized' if san else 'normal';exe=out/('noquick-'+mode)
   extra=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
   base=[x for x in flags if x not in ['-DTEST_NATIVE_TOOLBAR_QUICK','-DALARM_SERVICE_TAGGED_V2']]
   cmd=[os.environ.get('CC','cc'),*base,*extra,'-DPORTABLE_NOVA_UI','-DPORTABLE_WATCH_NATIVE_HEIGHT','-DPORTABLE_WATCH_COLOR_ONLY',str(test.with_name('shared_noquick_brightness_test.c')),'-Wl,--wrap=free','-o',str(exe)]
   receipt['commands'].append(cmd);subprocess.run(cmd,check=True)
   run=subprocess.run([exe],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=30,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'))
   log=out/('noquick-'+mode+'.log');log.write_text(run.stdout);receipt['results'].append({'profile':'noquick','mode':mode,'exit':run.returncode,'log':log.name})
   print('noquick',mode,'exit='+str(run.returncode),flush=True)
   if run.returncode:print(run.stdout);raise SystemExit(run.returncode)
 (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')

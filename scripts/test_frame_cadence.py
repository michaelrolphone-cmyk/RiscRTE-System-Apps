#!/usr/bin/env python3
"""Actual controller capability cadence and exact legacy-prefix allocation; no FPS claim."""
import argparse,json,os,pathlib,subprocess,hashlib
R=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
f=(R/'test/native_apps/springboard_nova_test.c').read_text().replace('#include "../../Apps/SpringboardPresentation.h"','#include "'+str(R/'Apps/SpringboardPresentation.h')+'"')
f=f.replace('static unsigned logical_presents,last_logical_present,launch_ms;','''static unsigned logical_presents,last_logical_present,launch_ms,min_interval=UINT32_MAX,poll_budget;
static unsigned expected(void){return (unsigned)atoi(getenv("EXPECTED"));}
static bool (*original_poll)(t5_app_input_t*,uint32_t);
static bool observed_poll(t5_app_input_t*out,uint32_t wait){assert(wait==(expected()<20?expected():20));poll_budget=wait;return original_poll(out,wait);}
''')
f=f.replace('if(logical_presents && scenario!=22)assert(ms-last_logical_present>=frame_test_interval());','if(logical_presents && scenario!=22){assert(ms-last_logical_present>=expected());if(ms-last_logical_present<min_interval)min_interval=ms-last_logical_present;}')
f=f.replace('original_present=source->present;observed.present=logical_present;', 'original_present=source->present;observed.present=logical_present;original_poll=source->poll;observed.poll=observed_poll;')
f=f.replace('o->nominal_refresh_millihz=scenario==22?0:60000;', 'o->nominal_refresh_millihz=(unsigned)atoi(getenv("NOMINAL"));if(getenv("RETAIN"))o->flags|=RISC_DISPLAY_INFO_RETAINS_IMAGE;')
f=f.replace('max_presents=ms/frame_test_interval()+1;','max_presents=ms/expected()+1;')
f=f.replace('assert(!!launched[0]==expect);','fprintf(stderr,"CADENCE %u %u %u %u %u %s\\n",expected(),poll_budget,min_interval,logical_presents,launch_ms,launched);assert(!!launched[0]==expect);')
f+='''
const springboard_presentation *cadence_real_get(void);
static unsigned private_period(void){return (unsigned)atoi(getenv("PRIVATE"));}
const springboard_presentation *springboard_presentation_get(void){
 const springboard_presentation *source=cadence_real_get();if(!source)return NULL;
 const char *mode=getenv("PREFIX");if(!mode)return source;
 static void *copy; if(!copy){size_t size=!strcmp(mode,"old")?offsetof(springboard_presentation,frame_period_ms):sizeof(*source);copy=malloc(size);assert(copy);memcpy(copy,source,size);((springboard_presentation*)copy)->struct_size=size;
 if(size==sizeof(*source))((springboard_presentation*)copy)->frame_period_ms=!strcmp(mode,"null")?NULL:private_period;}
 return copy;
}
'''
(a.output/'controller.c').write_text(f)
(a.output/'adapter.c').write_text('#define springboard_presentation_get cadence_real_get\n#include '+json.dumps(str(R/'lib/PortableApps/src/adapter.c'))+'\n')
records=[];commands=[]
variants=[('unknown',0,20,{}),('60hz',60000,17,{}),('30hz',30000,34,{}),('slow',10000,40,{}),('retain',60000,40,{'RETAIN':'1'}),('old',60000,40,{'PREFIX':'old'}),('null',60000,40,{'PREFIX':'null'}),('zero',60000,40,{'PREFIX':'private','PRIVATE':'0'}),('large',60000,40,{'PREFIX':'private','PRIVATE':'4294967295'}),('one',60000,1,{'PREFIX':'private','PRIVATE':'1'})]
for san in (False,True):
 flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
 base=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-I'+str(R/'lib/PortableApps/include'),'-I'+str(R/'lib/NativeApps/include')]
 cmd=base+[str(R/'test/native_apps/frame_cadence_test.c'),'-o',str(a.output/'pure')];subprocess.run(cmd,check=True);subprocess.run([a.output/'pure'],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});commands.append(cmd)
 cmd=base+['-DPORTABLE_NOVA_UI','-DPORTABLE_WATCH_NATIVE_HEIGHT','-DPORTABLE_WATCH_COLOR_ONLY','-DPORTABLE_FORCE_FULL_FRAMES','-DTEST_WIDTH=466','-DTEST_HEIGHT=466',str(R/'Apps/springboard.c'),str(a.output/'adapter.c'),str(a.output/'controller.c'),'-Wl,--wrap=t5_app_get_api','-o',str(a.output/'controller')];commands.append(cmd);subprocess.run(cmd,check=True)
 for name,nominal,interval,extra in variants:
  for case in ([0] if name=='one' else [0,6,14,15,23,24,26]):
   env={**os.environ,**extra,'NOMINAL':str(nominal),'EXPECTED':str(interval),'ASAN_OPTIONS':'detect_leaks=0'}
   result=subprocess.run([a.output/'controller',str(case)],env=env,text=True,capture_output=True,timeout=30)
   records.append({'sanitized':san,'variant':name,'case':case,'returncode':result.returncode,'stdout':result.stdout,'stderr':result.stderr})
# Compare exact action identity across cadence changes on the same Nova path.
for san in (False,True):
 reference={r['case']:r['stderr'].splitlines()[0].split()[6:] for r in records if r['sanitized']==san and r['variant']=='old' and not r['returncode']}
 for r in records:
  if r['sanitized']==san and r['variant'] not in ('slow','retain') and not r['returncode']:
   assert r['stderr'].splitlines()[0].split()[6:]==reference[r['case']],r
(a.output/'evidence.json').write_text(json.dumps({'scope':'Actual controller with modeled providers; scheduling target not hardware FPS','commands':commands,'runs':records,'source_revision':subprocess.check_output(['git','rev-parse','HEAD'],cwd=R,text=True).strip(),'dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=R,text=True)),'inputs':{str(p.relative_to(R)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [R/'Apps/SpringboardPresentation.h',R/'Apps/springboard_nova.inc',R/'lib/PortableApps/src/nova.inc',R/'lib/PortableApps/include/PortableFrameCadence.h']}},indent=2)+'\n')
fail=[r for r in records if r['returncode']];print(len(records),'controller lanes;',len(fail),'failures');
for r in fail:print(r['variant'],r['case'],r['stderr'][-300:])
if fail:raise SystemExit(1)

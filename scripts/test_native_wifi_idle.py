#!/usr/bin/env python3
import sys,json,hashlib,subprocess,pathlib,re,shutil,os
import argparse
ROOT=pathlib.Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description='Full persistence/availability idle discriminator with actual target-condition verification.');parser.add_argument('--output',type=pathlib.Path,required=True);args=parser.parse_args();OUT=args.output.resolve();OUT.mkdir(parents=True,exist_ok=False)
sys.path.insert(0,str(ROOT/'scripts'))
import test_native_wifi_features as features
import build_native_wifi_qualification as build

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def git(*a):return subprocess.check_output(['git','-C',str(ROOT),*a],text=True).strip()
LOCAL=git('rev-parse','HEAD'); TREE=git('rev-parse','HEAD^{tree}')
hashes=features.source_hashes(); switches=features.recipe_switches()
features.stage(OUT)
p=OUT/'portable_wifi_test.c';s=p.read_text()
needle='static bool fake_script_snapshot(risc_touch_snapshot_v1*out){'
s=s.replace(needle,needle+'if(probe_held){*out=(risc_touch_snapshot_v1){.width=width(),.height=height(),.contact_count=1,.contacts={{.id=1,.x=100,.y=100}}};return true;}')
needle='++sleeps;if(sleep_outcome==-2)'
assert s.count(needle)==1
s=s.replace(needle,'probe_sleep_read=raw_read;probe_sleep_written=raw_written;probe_sleep_contacts=raw_state.contact_count;probe_sleep_at=ticks;fprintf(stderr,"SLEEP_ENTRY at=%u raw_read=%u raw_written=%u held_contacts=%u\\n",ticks,raw_read,raw_written,raw_state.contact_count);'+needle)
s=s.replace('static void fake_yield(uint32_t ms){', 'static void fake_yield(uint32_t ms){if(probe_armed)fprintf(stderr,"DISPATCH at=%u cancelled=%u valid=%u down=%u released=%u progressed=%u reducer_sequence=%llu\\n",ticks,input_sample.cancelled,input_sample.valid,input_sample.down,input_sample.released,input_progressed,(unsigned long long)touch.sequence);')
s=s.replace('.timestamp_ms=ticks,.kind=kind,.id=id,.x=x,.y=y};}', '.timestamp_ms=ticks,.kind=kind,.id=id,.x=x,.y=y};fprintf(stderr,"RAW_CAPTURE at=%u sequence=%llu kind=%u\\n",ticks,(unsigned long long)raw_sequence,kind);}')
p.write_text(s)
source=features.HEADER+'''static bool probe_held,probe_armed;
static unsigned probe_sleep_read,probe_sleep_written,probe_sleep_contacts,probe_sleep_at;
#define WIFI_AVAILABILITY_LIBRARY
#include "'''+str(OUT/'wifi_availability_test.c')+'''"
int main(int argc,char**argv){
 assert(argc==2);unsigned mode=(unsigned)atoi(argv[1]);
 current_availability=RISC_KEY_VALUE_AVAILABLE;availability_start();limit_polls=1000000;
 assert(opened&&wifi_storage_ready()&&portable_wifi_idle_ready());
 ticks=last_activity+portable_idle_ms()-(mode==3?10:0);\n if(mode==1){probe_held=true;assert(fake_touch_poll(NULL,1));}
 render();assert(paper_frame_drain()&&portable_wifi_native_ready());
 assert(!surface.frame&&!paper_token&&display_settled&&!wn_gesture&&touch.cancel_pending&&!touch.down);
 if(mode==2||mode==3)probe_held=true;
 printf("BEFORE mode=%u at=%u deadline=%u ready=%u cancel=%u prefix=%u down=%u contact_count=%u raw_read=%u raw_written=%u\\n",mode,ticks,last_activity+portable_idle_ms(),portable_wifi_idle_ready(),touch.cancel_pending,touch.prefix,touch.down,touch.contact_count,raw_read,raw_written);
 probe_armed=true;t5_app_input_t in;assert(wa->poll(&in,0));
 printf("AFTER_FIRST mode=%u sleeps=%u raw_read=%u raw_written=%u activity=%u down=%u contacts=%u\\n",mode,sleeps,raw_read,raw_written,last_activity,touch.down,touch.contact_count);
 assert(!sleeps); /* Ordered cancellation must win the first poll, even when idle. */
 if(mode==0){assert(wa->poll(&in,0));assert(sleeps==1);puts("QUIET_CONTROL ordered cancellation then sleep PASS");}
 else{
  for(unsigned i=0;i<4;i++){ticks+=61000;assert(wa->poll(&in,0));assert(!sleeps&&touch.contact_count==1&&(unsigned)(ticks-last_activity)<=10);}
  assert(!writes&&!connects&&!text_opens);
  probe_held=false;for(unsigned i=0;i<4;i++)assert(wa->poll(&in,0));
  assert(!sleeps&&!touch.contact_count);ticks=last_activity+portable_idle_ms();
  for(unsigned i=0;i<4&&!sleeps;i++)assert(wa->poll(&in,0));
  assert(sleeps==1);puts("HELD_CONTROL no sleep while contact held; sleep after release and idle PASS");
 }
 return 0;
}
'''
(OUT/'probe.c').write_text(source)
# These are the complete delivered persistence/idle/controller switches supplied
# by the existing fixture plus selected native presentation switches.
passed=features.NATIVE_SWITCHES|features.FIXTURE_SWITCHES|features.ATOMIC_SWITCHES|{'PORTABLE_SETTINGS_AVAILABILITY'}
flags=[switches[n] for n in sorted(features.NATIVE_SWITCHES)]
includes=['-I'+str(ROOT/p) for p in ('lib/PortableApps/include','lib/NativeApps/include','lib/RemoteFiles/vendor/tinydtls_sha2')]
cc=str(pathlib.Path(shutil.which(os.environ.get('CC','cc'))).resolve())
base=[cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-DTEST_IDLE_ELIGIBILITY',*flags,*includes]
pre=base+['-dM','-E',str(OUT/'probe.c')]; macros_text=subprocess.check_output(pre,text=True)
(OUT/'host-macros.txt').write_text(macros_text);macros=dict(re.findall(r'^#define (\w+)(?: (.*))?$',macros_text,re.M))
for name in passed:
 assert name in macros,name
 v=switches[name][2:].split('=',1)
 if len(v)>1:assert re.sub(r'(?<=\d)[uUlL]+$','',macros[name])==v[1],name
 else:assert macros[name] in ('','1'),name
assert 'PORTABLE_X4_IDLE_POLICY' not in macros and 'PORTABLE_SLEEP_MANUAL_ONLY' not in macros
r={'local_source_commit':LOCAL,'source_tree':TREE,'recipe_sha256':build.RECIPE_SHA256,'scope':'Unchanged production Wi-Fi controller and adapter, complete atomic/availability idle-readiness feature subset; synthetic providers. Sleep helper entry observed, no physical device or power provider tested.','selected_recipe_subset':[switches[n] for n in sorted(passed)],'actual_host_macros':{n:macros[n] for n in switches if n in macros},'omitted_recipe_switches':[v for n,v in switches.items() if n not in macros],'preprocessor_command':pre,'compiler':{'path':cc,'sha256':sha(pathlib.Path(cc)),'version':subprocess.check_output([cc,'--version'],text=True).splitlines()[0]},'source_hashes':hashes,'fixture_hashes':{p.name:sha(p) for p in OUT.glob('*.c')},'tests':[]}
try:
 for sanitized in (False,True):
  label='sanitized' if sanitized else 'normal';binary=OUT/label
  cmd=base+(['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie'] if sanitized else [])+[str(OUT/'probe.c'),str(ROOT/'lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c'),'-o',str(binary)]
  subprocess.run(cmd,check=True,timeout=120)
  binary_sha=sha(binary)
  for mode in range(4):
   run=subprocess.run([str(binary),str(mode)],text=True,capture_output=True,timeout=30,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
   log=OUT/(label+'-'+str(mode)+'.log');log.write_text(run.stdout+run.stderr)
   print(label,mode,run.returncode,run.stdout+run.stderr,flush=True)
   r['tests'].append({'sanitized':sanitized,'case':mode,'command':cmd,'binary_sha256':binary_sha,'returncode':run.returncode,'log_sha256':sha(log)})
   run.check_returncode()
  binary.unlink()
 assert hashes==features.source_hashes()
 r['status']='PASS'
finally:(OUT/'receipt.json').write_text(json.dumps(r,indent=2)+'\n')

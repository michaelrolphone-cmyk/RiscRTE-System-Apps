#!/usr/bin/env python3
"""Full Files and Home controller composition with explicit Runtime/hardware endpoints."""
import argparse,hashlib,json,os,subprocess,sys,shlex
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for k in ('home','home-receipt','home-fixture','composed-commands','include','system','utilities','output'):p.add_argument('--'+k,type=Path,required=True)
p.add_argument('--sanitize',action='store_true');a=p.parse_args();o=a.output.resolve();o.mkdir(parents=True,exist_ok=False);commands=[];inputs={}
sys.path.insert(0,str(a.home.resolve()/'scripts'));from merged_sdk import headers
sdk=headers(o/'home-sdk',a.system.resolve(),a.utilities.resolve(),a.home.resolve())
def run(cmd):
 cmd=list(map(str,cmd));r=subprocess.run(cmd,text=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},timeout=180);commands.append(dict(command=cmd,returncode=r.returncode,output=r.stdout+r.stderr));(o/'commands.json').write_text(json.dumps(commands,indent=2)+'\n');print(r.stdout+r.stderr,end='');r.check_returncode()
def record(cmd):
 sources=[Path(t) for t in cmd if Path(t).suffix in ('.c','.cpp')];prefix=[];skip=False
 for t in cmd[1:]:
  if skip:skip=False;continue
  if t in ('-o','-MF'):skip=True;continue
  if t in ('-c','-shared','-MMD') or Path(t).suffix in ('.o','.c','.cpp') or t.startswith('-Wl,'):continue
  prefix.append(t)
 for s in sources:
  raw=subprocess.check_output([cmd[0],*prefix,'-MM',str(s)],text=True)
  for n in shlex.split(raw.replace('\\\n',' ').split(':',1)[1]):
   f=Path(n).resolve();inputs[str(f)]=hashlib.sha256(f.read_bytes()).hexdigest()
for p in (Path(__file__),a.home_receipt,a.home_fixture,a.composed_commands,ROOT/'test/files_home/home_bridge.inc',ROOT/'test/files_home/root_bridge.inc',ROOT/'test/files_scene/composed_root_test.c'):
 inputs[str(p.resolve())]=hashlib.sha256(p.read_bytes()).hexdigest()
text=a.home_fixture.read_text();needle='static bool effect_get(risc_resident_client_v1*c)';assert text.count(needle)==1
text=text.replace(needle,'static int32_t files_home_run(uint64_t,const char*,risc_resident_result_v1*);\n'+needle).replace('c->run_foreground=effect_run;','c->run_foreground=files_home_run;').replace('int main(int argc,char**argv){','int inherited_effects_main(int argc,char**argv){')
text+='\n'+(ROOT/'test/files_home/home_bridge.inc').read_text();hf=o/'home.c';hf.write_text(text)
home_commands=json.loads(a.home_receipt.read_text())['commands']
old_system=next(t.split('/lib/PortableApps/')[0] for cmd in home_commands for t in cmd if t.endswith('/lib/PortableApps/src/quick_actions.c'))
for cmd in home_commands:
 new=[]
 for t in cmd:
  t=t.replace(old_system,str(a.system.resolve()))
  if t.startswith('-I') and '/resident-alarm-full-' in t:t='-I'+str(sdk)
  elif '/resident-alarm-full-' in t:t=str(hf) if t.endswith('fixture.c') else str(o/('home-'+Path(t).name))
  new.append(t)
 new+=['-fPIC']
 if a.sanitize:new+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
 if '-c' not in new:new[new.index('-o')+1]=str(o/'home.so');new+=['-shared','-Wl,-Bsymbolic']
 record(new);run(new)
text=(ROOT/'test/files_scene/composed_root_test.c').read_text().replace('int main(int argc,char**argv){','int files_child_main(int argc,char**argv){')
bridge='int files_child_main(int,char**);\n'+(ROOT/'test/files_home/root_bridge.inc').read_text()
text=text.replace('#include "../../Apps/files_scene.c"','#include "'+str(ROOT/'Apps/files_scene.c')+'"\n'+bridge).replace('#include "../scene/host_test.c"','#include "'+str(ROOT/'test/scene/host_test.c')+'"')
text=text.replace('*out=(contexts_status_v1){.struct_size=sizeof(*out)};', '*out=(contexts_status_v1){.struct_size=sizeof(*out)};out->audio=(contexts_source_status_v1){.source=CONTEXTS_AUDIO,.room_entry=1,.preset_entry=preset_claims?1:0,.model_generation=3,.room_slot=0,.current=true,.room_valid=true};strcpy(out->audio.room_name,"office");')
text=text.replace('(void)generation;assert(0);return false;', '(void)generation;++preset_claims;assert(preset_claims==1);return true;')
text=text.replace('(void)result;assert(0);return false;', '(void)result;++preset_reports;assert(preset_claims==1&&preset_writes==expected_writes&&result==CONTEXTS_PRESET_APPLIED);return true;')
text=text.replace('return RISC_KEY_VALUE_NOT_FOUND;}\nstatic int32_t context_put', 'return preset_get(k,b,cap,n);}\nstatic int32_t context_put')
text=text.replace('(void)c;(void)k;(void)b;(void)n;assert(0);return -1;', '(void)c;++preset_writes;assert(preset_claims==1&&!preset_reports&&!files_scene_background_capture_active());return home_put(k,b,n);')
text=text.replace('p=&context_service;', 'p=radio_rule_case?&rules_service.base:&context_service;')
text=text.replace('root_runtime=(risc_runtime_api_v1)', 'rules_service=(contexts_fingerprint_service_v1){.base=context_service,.fingerprint_abi=0x31504643u,.fingerprint=rules_fingerprint};rules_service.base.struct_size=sizeof(rules_service);root_runtime=(risc_runtime_api_v1)')
text=text.replace('p->enabled&&p->audio_allowed&&p->radio_allowed&&p->awake','p->enabled&&p->audio_allowed&&(radio_rule_case||p->radio_allowed)&&p->awake')
text=text.replace('if(!p)return false;', 'if(radio_rule_case&&!strcmp(n,RISC_SHARED_DATA_CAPABILITY)){assert(instance>=2&&instance<=4);p=&rules_data;}if(!strcmp(n,"alarm.service")&&version==1){p=home_alarm();}if(!strcmp(n,"display.output")){composed_display=disp;composed_display.get_info=composed_info;composed_display.set_brightness=composed_brightness;p=&composed_display;}\n if(!p)return false;')
text=text.replace('.retain_invocation=root_retain}', '.retain_invocation=root_retain,.resident_shell=composed_resident}')
text=text.replace('&&(!contexts_case||context_captures>20)', '&&(!contexts_case||context_captures>20)&&settings_calls==expected_calls&&!files_scene_background_effects()')
text=text.replace('app_grants,submits);\n}', 'app_grants,submits);return 0;\n}')
rf=o/'root.c';rf.write_text(text)
for row in json.loads(a.composed_commands.read_text()):
 cmd=row['command']
 if cmd[0] not in ('cc','c++'):continue
 new=[]
 for t in cmd:
  if t.endswith('/test/files_scene/composed_root_test.c'):t=str(rf)
  elif t.startswith('-I') and t.endswith('/include'):t='-I'+str(a.include.resolve())
  elif t.startswith(str(a.composed_commands.parent.resolve())):t=str(o/Path(t).name)
  new.append(t)
 if a.sanitize:new+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 if '-c' not in new:new+=['-ldl']
 record(new);run(new)
for case in ('brightness','pending','clean-failure','radios'):run([o/'test',o/'home.so',case])
assert inputs=={p:hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in inputs}
(o/'receipt.json').write_text(json.dumps(dict(scope=__doc__,compiled_inputs=inputs,sanitized=a.sanitize,hardware_tested=False,actual_runtime=False),indent=2)+'\n')

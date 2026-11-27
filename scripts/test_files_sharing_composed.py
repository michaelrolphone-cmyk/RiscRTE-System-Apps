#!/usr/bin/env python3
"""Complete Files root and sharing/setup controllers; radio/network/storage endpoints doubled."""
import argparse,hashlib,json,os,shlex,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for k in ('commands','runtime','output'):p.add_argument('--'+k,type=Path,required=True)
p.add_argument('--sanitize',action='store_true');a=p.parse_args();o=a.output.resolve();o.mkdir(parents=True,exist_ok=False)
text=(ROOT/'test/files_scene/composed_root_test.c').read_text().replace('#include "../scene/host_test.c"','#include "'+str(ROOT/'test/scene/host_test.c')+'"').replace('#include "../../Apps/files_scene.c"','#include "'+str(ROOT/'Apps/files_scene.c')+'"')
endpoint=(ROOT/'test/native_apps/file_browser_sharing_test.c').read_text()
endpoint=endpoint[endpoint.index('static bool policy_off'):endpoint.index('#ifdef PORTABLE_CHECKED_STORAGE_RUNTIME')]
endpoint=endpoint.replace('#include "file_browser_setup_fixture.inc"',(ROOT/'test/native_apps/file_browser_setup_fixture.inc').read_text())
endpoint=endpoint.replace('unsigned calls,','unsigned sharing_calls,').replace('++calls','++sharing_calls').replace('native_custody_retained','files_retained').replace('test_ticks','ms')
endpoint=endpoint.replace('assert(direct_ble_on);','assert(!fsh_direct||direct_ble_on);')
endpoint=endpoint.replace('static risc_app_data_export_v1 export_api;','').replace(',retains;', ';').replace('direct_ble_on,direct_ble_initial_on,','direct_ble_on,').replace(',setup_releases,',',')
endpoint='#include \"RiscTcpConnectionV1.h\"\n#define FILE_SETUP_TEST\n#define FILE_DIRECT_FIXTURE\n'+endpoint
text=text.replace('static risc_runtime_api_v1 root_runtime;',endpoint+'\nstatic risc_runtime_api_v1 root_runtime;')
text=text.replace('if(!p)return false;','if(!strcmp(n,"net.wifi")){p=&wifi;}if(!strcmp(n,RISC_KEY_VALUE_CAPABILITY)){p=&kv;}if(!strcmp(n,RISC_TCP_CONNECTION_CAPABILITY)){p=&tcp;}if(!strcmp(n,RISC_ENTROPY_SOURCE_CAPABILITY)){p=&entropy;}if(!strcmp(n,"bluetooth.hci")){p=&direct_ble;}if(!strcmp(n,RISC_BLUETOOTH_SESSION_SETUP_CAPABILITY)){p=&setup_api;}if(!p)return false;')
start=text.index('static void root_yield(');end=text.index('int main(',start);text=text[:start]+(ROOT/'test/files_scene/sharing_composed.inc').read_text()+text[end:]
text=text.replace(' assert(argc==3);',' assert(argc==3);poll_step=10;share_direct=strstr(argv[1],"direct")!=NULL;share_reject=strstr(argv[1],"reject")!=NULL;share_setup=!strcmp(argv[1],"lan-setup");borrowed=!share_direct;')
text=text.replace('static risc_storage_volume_api_v1 volume;', 'static risc_storage_volume_api_v1 volume;static risc_storage_volume_api_v1_state share_volume;')
text=text.replace('p=&volume;', 'p=&share_volume.prepared.base.sleep.terminal.power.volume.base;')
text=text.replace('volume_present=!strcmp(argv[1],"volume-unready");','volume_present=true;')
text=text.replace(' volume=fbm_volume;', ' volume=fbm_volume;')
a0=text.index(' volume=fbm_volume;');b0=text.index(' root_runtime=',a0)
text=text[:a0]+' volume=(risc_storage_volume_api_v1){.api_version=1,.struct_size=sizeof(volume),.refresh=share_ready,.ready=share_ready,.label=root_label,.stat=share_stat,.dir_open=share_dir,.dir_next=share_next,.dir_close=share_dir_close,.file_open_read=root_read_open,.file_read=root_read,.file_close=root_file_close,.last_error=share_error};\n'+text[b0:]
text=text.replace(' root_runtime=', ' share_volume.prepared.base.sleep.terminal.power.volume.base=volume;share_volume.prepared.base.sleep.terminal.power.volume.base.struct_size=sizeof(share_volume);share_volume.state_tag=RISC_STORAGE_STATE_TAG;share_volume.state_version=1;share_volume.observe=share_observe;\n root_runtime=')
text=text.replace(' assert(files_clean&&',' fprintf(stderr,\"sharing final clean=%u retained=%u token=%llu stage=%u share=%u phase=%u grants=%u/%u setup=%u confirms=%u\\n\",files_clean,files_retained,(unsigned long long)files_ui.token,stage,share_stage,fsh_phase,app_grants,app_releases,setup_state,setup_confirms);\n assert(files_clean&&')
text=text.replace('submits&&stage==3','submits&&share_stage==4')
f=o/'fixture.c';f.write_text(text);rows=[];inputs={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [Path(__file__).resolve(),ROOT/'test/files_scene/sharing_composed.inc',ROOT/'test/files_scene/composed_root_test.c',a.commands.resolve(),f,ROOT/'test/native_apps/file_browser_sharing_test.c',ROOT/'test/native_apps/file_browser_setup_fixture.inc']}
def run(cmd):
 cmd=list(map(str,cmd));r=subprocess.run(cmd,capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},timeout=120);rows.append(dict(command=cmd,returncode=r.returncode,output=r.stdout+r.stderr));(o/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');print(r.stdout+r.stderr,end='');r.check_returncode()
for row in json.loads(a.commands.read_text()):
 cmd=row['command']
 if cmd[0] not in ('cc','c++'):continue
 new=[str(f) if t.endswith('/test/files_scene/composed_root_test.c') else str(o/Path(t).name) if t.startswith(str(a.commands.parent.resolve())) else t for t in cmd]
 if a.sanitize:new+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 run(new)
for dep in o.glob('*.d'):
 for name in shlex.split(dep.read_text().replace('\\\n',' ').split(':',1)[1]):
  q=Path(name).resolve();inputs[str(q)]=hashlib.sha256(q.read_bytes()).hexdigest()
for profile in ('watch','paper','gray'):
 for case in ('lan','lan-setup','direct','direct-reject'):run([o/'test',case,profile])
assert inputs=={p:hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in inputs}
(o/'receipt.json').write_text(json.dumps(dict(scope=__doc__,cases=12,sanitized=a.sanitize,compiled_inputs=inputs,hardware_tested=False,actual_runtime=False),indent=2)+'\n')

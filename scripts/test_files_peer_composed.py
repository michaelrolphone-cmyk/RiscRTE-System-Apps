#!/usr/bin/env python3
"""Complete Files root, scene and peer/WebDAV controllers; radio/network endpoints doubled."""
import argparse,hashlib,json,os,shlex,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for k in ('commands','runtime','output'):p.add_argument('--'+k,type=Path,required=True)
p.add_argument('--sanitize',action='store_true');a=p.parse_args();o=a.output.resolve();o.mkdir(parents=True,exist_ok=False)
text=(ROOT/'test/files_scene/composed_root_test.c').read_text().replace('#include "../scene/host_test.c"','#include "'+str(ROOT/'test/scene/host_test.c')+'"').replace('#include "../../Apps/files_scene.c"','#include "'+str(ROOT/'Apps/files_scene.c')+'"')
text=text.replace('static risc_runtime_api_v1 root_runtime;','static risc_runtime_api_v1 root_runtime;\nextern void peer_backend_begin(void),peer_backend_tick(uint64_t),peer_backend_end(void),peer_backend_direct(void);extern bool peer_backend_remote_exists(const char*);static bool peer_preview_case,peer_delete_case,peer_source_case;static const risc_file_open_source_api_v1 peer_source_api;extern const void*peer_composed_capability(const char*);')
text=text.replace('if(!p)return false;','if(!p){p=peer_composed_capability(n);}if(peer_source_case&&!strcmp(n,RISC_FILE_OPEN_SOURCE_CAPABILITY)){p=&peer_source_api;}if(!strcmp(n,RISC_KEY_VALUE_CAPABILITY))p=&context_preferences;if(!p)return false;')
text=text.replace('*n=0;if(!strcmp(k,"contexts_on"))','*n=0;if(!strcmp(k,PORTABLE_RADIO_KEY)){assert(cap>=4);memcpy(b,(uint8_t[]){0x51,1,3,0xa6},4);*n=4;return 0;}if(!strcmp(k,"contexts_on"))')
start=text.index('static void root_yield(');end=text.index('int main(',start);text=text[:start]+(ROOT/'test/files_scene/peer_composed.inc').read_text()+text[end:]
text=text.replace(' assert(argc==3);',' assert(argc==3);peer_preview_case=!strcmp(argv[1],"preview");peer_delete_case=!strcmp(argv[1],"delete");peer_source_case=!strcmp(argv[1],"source");peer_reject=!strcmp(argv[1],"reject");peer_direct=!strcmp(argv[1],"direct");peer_backend_begin();if(peer_direct)peer_backend_direct();')
text=text.replace('assert(driver->quiesce());','peer_backend_end();assert(driver->quiesce());')
f=o/'fixture.c';f.write_text(text);rows=[];inputs={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [Path(__file__).resolve(),ROOT/'test/files_scene/peer_composed.inc',ROOT/'test/files_scene/composed_root_test.c',a.commands.resolve(),f]}
def run(cmd):
 cmd=list(map(str,cmd));r=subprocess.run(cmd,capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},timeout=120);rows.append(dict(command=cmd,returncode=r.returncode,output=r.stdout+r.stderr));(o/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');print(r.stdout+r.stderr,end='');r.check_returncode()
backend=o/'backend.cpp';backend.write_text('#include "'+str(ROOT/'test/remote_files/peer_ui_backend.cpp')+'"\nextern "C" const void*peer_composed_capability(const char*n){if(!strcmp(n,"net.wifi"))return &backend->wifi;return peer_backend_capability(n); }\n')
extra=[];common=['-O0','-g1','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',*[v for v in json.loads(a.commands.read_text())[0]['command'] if v.startswith('-I')],'-I'+str(a.runtime/'src'),'-I'+str(ROOT/'lib/RemoteFiles'),'-I'+str(ROOT/'lib/UserFiles')]
if a.sanitize:common+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
for i,src in enumerate([backend,ROOT/'lib/UserFiles/UserFilesVolume.cpp']):
 obj=o/f'backend{i}.o';run(['c++','-std=c++17',*common,'-MMD','-MF',o/f'backend{i}.d','-c',src,'-o',obj]);extra.append(obj)
for row in json.loads(a.commands.read_text()):
 cmd=row['command']
 if cmd[0] not in ('cc','c++'):continue
 new=[str(f) if t.endswith('/test/files_scene/composed_root_test.c') else str(o/Path(t).name) if t.startswith(str(a.commands.parent.resolve())) else t for t in cmd]
 if a.sanitize:new+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 if '-c' not in new:new+=list(map(str,extra))
 run(new)
for dep in o.glob('*.d'):
 for name in shlex.split(dep.read_text().replace('\\\n',' ').split(':',1)[1]):
  q=Path(name).resolve();inputs[str(q)]=hashlib.sha256(q.read_bytes()).hexdigest()
for profile in ('watch','paper','gray'):
 for case in ('lan','direct','reject','preview','delete','source'):run([o/'test',case,profile])
assert inputs=={p:hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in inputs}
(o/'receipt.json').write_text(json.dumps(dict(scope=__doc__,cases=18,sanitized=a.sanitize,compiled_inputs=inputs,hardware_tested=False,actual_runtime=False),indent=2)+'\n')

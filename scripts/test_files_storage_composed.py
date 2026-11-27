#!/usr/bin/env python3
"""Native scene-driven storage, text and source/legacy handoff through the complete Files root."""
import argparse,hashlib,json,os,shlex,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--commands',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();o=a.output.resolve();o.mkdir(parents=True,exist_ok=False)
text=(ROOT/'test/files_scene/composed_root_test.c').read_text().replace('#include "../scene/host_test.c"','#include "'+str(ROOT/'test/scene/host_test.c')+'"').replace('#include "../../Apps/files_scene.c"','#include "'+str(ROOT/'Apps/files_scene.c')+'"')
text=text.replace('static risc_runtime_api_v1 root_runtime;', 'static risc_runtime_api_v1 root_runtime;\nstatic risc_storage_volume_api_v1_ext storage_extended;\nstatic const t5_file_open_api_v1 storage_handlers;\nstatic const risc_file_open_source_api_v1 storage_source;\nstatic unsigned storage_source_fault;\nstatic bool storage_source_case,storage_source_busy_case,storage_source_cancel_case,storage_source_identity_case;\nextern const risc_driver_v2*files_text_driver_get(uint32_t);static const risc_driver_v2*storage_text_driver;')
text=text.replace('if(!p)return false;', 'if(storage_source_case&&!strcmp(n,RISC_FILE_OPEN_SOURCE_CAPABILITY)){assert(version==1&&!instance);p=&storage_source;}if(!strcmp(n,"file.open")){assert(version==1&&!instance);p=&storage_handlers;}if(!strcmp(n,"ui.text-input")){assert(version==1&&!instance);p=storage_text_driver->capability;}if(!p)return false;')
text=text.replace('assert(driver->start(deps,5));','assert(driver->start(deps,5));storage_text_driver=files_text_driver_get(2);risc_provider_dependency_v1 text_dep[]={{"ui.scene",1,api},{"usb.hid.keyboard",1,&storage_keyboard}};assert(storage_text_driver->start(text_dep,(storage_text_accept||storage_new_folder)?2:1));')
text=text.replace('assert(driver->quiesce());','assert(storage_text_driver->quiesce());storage_text_driver->stop();assert(driver->quiesce());')
text=text.replace('p=&volume;', 'p=storage_extended.base.struct_size?&storage_extended.base:&volume;')
start=text.index('static void root_yield(');end=text.index('int main(',start)
text=text[:start]+'static void storage_input(void);\nstatic void root_yield(uint32_t n){ms+=n;assert(++yields<10000);storage_input();}\n'+(ROOT/'test/files_scene/storage_composed.inc').read_text()+text[end:]
text=text.replace('volume_present=!strcmp(argv[1],"volume-unready");','volume_present=true;storage_source_fault=!strcmp(argv[1],"source-dirty-begin")?6:!strcmp(argv[1],"source-dirty-write")?7:!strcmp(argv[1],"source-invalid-finish")?8:!strcmp(argv[1],"source-begin-identity")?1:!strcmp(argv[1],"source-read-identity")?2:!strcmp(argv[1],"source-stat-identity")?3:!strcmp(argv[1],"source-finish-identity")?4:!strcmp(argv[1],"source-open-identity")?5:0;storage_source_identity_case=!strcmp(argv[1],"source-identity");storage_source_case=!strncmp(argv[1],"source",6);storage_source_busy_case=!strcmp(argv[1],"source-busy");storage_source_cancel_case=!strcmp(argv[1],"source-cancel");storage_handler_case=strstr(argv[1],"handler")!=NULL;storage_handler_paged=!strcmp(argv[1],"handlers-page");storage_text_accept=!strcmp(argv[1],"rename");storage_new_folder=!strcmp(argv[1],"new-folder");storage_text_cancel=!strcmp(argv[1],"rename-cancel");storage_copy=!strcmp(argv[1],"copy");storage_move=!strcmp(argv[1],"move");storage_cancel=!strcmp(argv[1],"cancel");storage_preview=!strcmp(argv[1],"preview");')
start=text.index(' volume=fbm_volume;');end=text.index(' root_runtime=',start)
text=text[:start]+''' volume=(risc_storage_volume_api_v1){.api_version=1,.struct_size=sizeof(volume),.refresh=storage_ready,.ready=storage_ready,.label=root_label,.stat=storage_stat,.dir_open=storage_dir,.dir_next=storage_next,.dir_close=storage_dir_close,.file_open_read=storage_open,.file_read=storage_read,.file_close=storage_close,.file_open_write=storage_create,.file_write=storage_write,.remove=storage_remove,.last_error=storage_error};
'''+(' storage_extended=(risc_storage_volume_api_v1_ext){.base=volume,.rename=storage_rename,.mkdir=storage_mkdir};storage_extended.base.struct_size=sizeof(storage_extended);\n' if True else '')+text[end:]
text=text.replace('assert(driver->quiesce());','assert(!storage_usb&&!storage_reader_open&&!storage_file_open&&!storage_directory_open);assert(storage_exists==(storage_preview||storage_cancel||storage_copy||storage_text_cancel||storage_new_folder||storage_handler_case||storage_source_case));\n assert(driver->quiesce());')
text=text.replace('assert(0&&"clean composition must not retain");', 'assert(storage_source_fault>=6);')
text=text.replace(' app_main();',' app_main();if(storage_source_identity_case||storage_source_fault){assert(files_retained&&!files_clean&&fsource_token==17&&!storage_source_discards);if(storage_source_fault<4||storage_source_fault==6||storage_source_fault==7)assert(fsource_input==2&&storage_reader_open&&!storage_source_finishes&&!storage_source_requests);else assert(!fsource_input&&!storage_reader_open);puts("Source identity loss retains exact token/read custody without further callbacks PASS");return 0;}')
fixture=o/'fixture.c';fixture.write_text(text);rows=[];inputs={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [Path(__file__).resolve(),ROOT/'test/files_scene/storage_composed.inc',ROOT/'test/files_scene/composed_root_test.c',a.commands.resolve(),fixture]}
def run(cmd):
 cmd=list(map(str,cmd));r=subprocess.run(cmd,capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},timeout=120);rows.append(dict(command=cmd,returncode=r.returncode,output=r.stdout+r.stderr));(o/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');print(r.stdout+r.stderr,end='');r.check_returncode()
text_object=o/'text-provider.o'
text_cmd=['cc','-std=c11','-O0','-g1','-Wall','-Wextra','-Werror','-fPIC','-Dt5_driver_get=files_text_driver_get',*[v for v in json.loads(a.commands.read_text())[0]['command'] if v.startswith('-I')],'-MMD','-MF',str(o/'text-provider.d'),'-c',str(ROOT/'Services/text_input/host.c'),'-o',str(text_object)]
if a.sanitize:text_cmd+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
run(text_cmd)
for row in json.loads(a.commands.read_text()):
 cmd=row['command']
 if cmd[0] not in ('cc','c++'):continue
 new=[str(fixture) if t.endswith('/test/files_scene/composed_root_test.c') else str(o/Path(t).name) if t.startswith(str(a.commands.parent.resolve())) else t for t in cmd]
 if a.sanitize:new+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 if '-c' not in new:new.append(str(text_object))
 run(new)
for dep in o.glob('*.d'):
 for name in shlex.split(dep.read_text().replace('\\\n',' ').split(':',1)[1]):
  f=Path(name).resolve();inputs[str(f)]=hashlib.sha256(f.read_bytes()).hexdigest()
for profile in ('watch','paper','gray'):
 for case in ('preview','delete','cancel','copy','move','rename-cancel','rename','new-folder','handler','handlers-page','source','source-busy','source-cancel','source-identity','source-begin-identity','source-read-identity','source-stat-identity','source-finish-identity','source-open-identity','source-dirty-begin','source-dirty-write','source-invalid-finish'):run([o/'test',case,profile])
assert inputs=={p:hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in inputs}
(o/'receipt.json').write_text(json.dumps(dict(scope=__doc__,cases=66,sanitized=a.sanitize,compiled_inputs=inputs,hardware_tested=False,actual_runtime=False),indent=2)+'\n')

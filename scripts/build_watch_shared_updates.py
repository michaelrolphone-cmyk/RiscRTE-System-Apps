#!/usr/bin/env python3
"""Build shared native-height update applications with inherited Watch features."""
import argparse,hashlib,json,subprocess,sys
from pathlib import Path
r=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser()
for n in ('watch','utilities','runtime','baseline','output'):p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
from build_contexts_cohort import definitions
from build_current_apps import application_inputs
from compact_current_elf import compact
cc=str(Path.home()/'.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc')
watch=a.watch.resolve();util=a.utilities.resolve();runtime=a.runtime.resolve()
assert subprocess.check_output(['git','-C',str(watch),'rev-parse','HEAD'],text=True).strip()=='6e82fdcaec0b73ed700d2104cfb4d94f011c19d4'
(out/'empty_catalog.c').write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
alarm=out/'watch-alarm-profile.h';alarm.write_text('#include "'+str(util/'lib/Alarm/include/AlarmServiceV1.h')+'"\n#define ALARM_MODE_VISUAL 0u\nstatic inline uint32_t alarm_service_output_modes(const alarm_service_v1*s){return s?ALARM_MODE_BOTH:ALARM_MODE_VISUAL;}\n')
validator=out/'validate-elf'
subprocess.run(['cc','-std=c11','-I'+str(r/'test/native_apps/stubs'),'-I'+str(r/'lib/elf_loader/include'),str(r/'lib/elf_loader/src/esp_elf_validate.c'),str(r/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
for name in ('ota_update','app_store'):
 flags=definitions(name,'1.1.6',watch)+['-include',str(alarm),'-DPORTABLE_NATIVE_CUSTODY_FENCE','-DPORTABLE_ALARM_TERMINAL_RETENTION','-DPORTABLE_CHECKED_STORAGE_RUNTIME','-DPORTABLE_DISPLAY_ROTATION=0','-DPORTABLE_NAVIGATION_RETENTION','-DPORTABLE_ASYNC_RGB565','-DPORTABLE_WATCH_NATIVE_HEIGHT','-flto','-mno-target-align']
 sources,includes,allowed=application_inputs(name,r/'Apps'/(name+'.c'),{'system-apps':r,'utilities':util,'runtime':runtime},watch,out,'runtime-features')
 includes += [util/'lib/Contexts/include',runtime/'sdk/app']
 mapping=out/(name+'.map');mapping.write_text('{global:app_main;app_module_init;app_module_fini;local:*;};\n');elf=out/(name+'.elf')
 cmd=[cc,'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wl,--version-script='+str(mapping),'-Wall','-Wextra','-Werror',*flags,*['-I'+str(i) for i in includes],*map(str,sources),'-lgcc','-o',str(elf)]
 subprocess.run(cmd,check=True);proof=compact(elf,cc,debug_path=out/'debug'/(name+'.elf'));subprocess.run([str(validator),str(elf)],check=True)
 syms=subprocess.check_output([cc.removesuffix('gcc')+'nm','-D',str(elf)],text=True);imports={x.split()[-1] for x in syms.splitlines() if ' U ' in ' '+x};assert imports<=allowed,imports-allowed
 exports={x.split()[-1] for x in syms.splitlines() if len(x.split())>=3 and x.split()[-2] in ('T','D','B','R')};assert exports=={'app_main','app_module_init','app_module_fini'}
 manifest=json.loads((a.baseline/(name+'.json')).read_text());manifest['version']='1.1.6';(out/(name+'.json')).write_text(json.dumps(manifest,indent=2)+'\n')
 record={'sources':{n:{'commit':subprocess.check_output(['git','-C',str(repo),'rev-parse','HEAD'],text=True).strip(),'tracked_dirty':bool(subprocess.check_output(['git','-C',str(repo),'diff','HEAD','--name-only'],text=True).strip())} for n,repo in {'system':r,'watch':watch,'utilities':util,'runtime':runtime}.items()},'command':cmd,'compaction':proof,'imports':sorted(imports),'exports':sorted(exports),'bytes':elf.stat().st_size,'sha256':hashlib.sha256(elf.read_bytes()).hexdigest(),'source_sha256':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for tree in (r/'Apps',r/'lib/PortableApps') for f in tree.rglob('*') if f.is_file()},'hardware_verified':False}
 (out/(name+'-build.json')).write_text(json.dumps(record,indent=2)+'\n');print(name,record['bytes'],record['sha256'],flush=True)

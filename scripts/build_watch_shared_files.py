#!/usr/bin/env python3
"""Build only shared Files against the full Watch canvas cohort's app contract.
Does not assemble firmware or alter a baseline store. Product admission remains
an explicit integrator step using the emitted immutable authority requirements.
"""
import argparse,hashlib,json,os,shutil,subprocess,sys
from pathlib import Path
from types import SimpleNamespace
import portable_file_sharing_build as sharing
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for n in ('watch','utilities','runtime','baseline','output'):p.add_argument('--'+n,type=Path,required=True)
p.add_argument("--peer-files",action="store_true")
p.add_argument("--source-handoff",action="store_true")
p.add_argument("--volume-instance",type=int,help="Optional third generic storage.volume root instance")
p.add_argument("--shared-text-input",action="store_true")
p.add_argument("--shared-text-policy",action="store_true",help="Service lifecycle policy inside shared Files editing")
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
if a.shared_text_policy and not a.shared_text_input:p.error('--shared-text-policy requires --shared-text-input')
watch=a.watch.resolve();utilities=a.utilities.resolve();runtime=a.runtime.resolve()
sys.path.insert(0,str(watch/'scripts'))
from build_current_apps import application_inputs
from build_contexts_cohort import definitions
cc=os.environ.get('NATIVE_APP_CC') or str(Path.home()/'.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc')
manifest=json.loads((a.baseline/'file_browser.json').read_text())
assert manifest['id']=='file_browser'
boot=json.loads((a.baseline/'boot.json').read_text())
old=next(x for x in boot['app_capabilities'] if x['manifest']=='file_browser.json')
# Keep every Watch feature and exact existing grant, including Contexts shared
# namespaces, motion wake, telemetry, local Quick Actions and local sleep.
watch_commit=subprocess.check_output(['git','-C',str(watch),'rev-parse','HEAD'],text=True).strip()
assert watch_commit=='6e82fdcaec0b73ed700d2104cfb4d94f011c19d4','Watch dependency changed; requalify'
flags=definitions('file_browser',sharing.selected_version(a),watch)
flags+=['-DPORTABLE_CHECKED_STORAGE_RUNTIME']
# Watch v1's appended sleep callback is not X4's output-mode descriptor. Use
# the exact Watch header and preserve its existing both-output semantics.
alarm=out/'watch-alarm-profile.h'
alarm.write_text('#include "'+str(utilities/'lib/Alarm/include/AlarmServiceV1.h')+'"\n#define ALARM_MODE_VISUAL 0u\nstatic inline uint32_t alarm_service_output_modes(const alarm_service_v1 *s){return s?ALARM_MODE_BOTH:ALARM_MODE_VISUAL;}\n')
flags+=['-include',str(alarm)]
flags+=['-DPORTABLE_FILE_BROWSER_RGB_ONLY','-DPORTABLE_DISPLAY_ROTATION=0','-DPORTABLE_WIFI_INSTANCE=15','-DPORTABLE_FILE_BROWSER_HANDLERS','-DPORTABLE_FILE_BROWSER_SECONDARY_CAPABILITY="storage.app-data.export"','-DPORTABLE_FILE_BROWSER_SECONDARY_INSTANCE=0']
if a.volume_instance is not None:flags+=['-DPORTABLE_FILE_BROWSER_TERTIARY_CAPABILITY="storage.volume"','-DPORTABLE_FILE_BROWSER_TERTIARY_INSTANCE='+str(a.volume_instance)]
if a.source_handoff:flags+=['-DPORTABLE_FILE_BROWSER_SOURCE_HANDOFF']
if a.shared_text_input:flags+=['-DPORTABLE_FILE_BROWSER_SHARED_INPUT','-DPORTABLE_TEXT_INPUT_CLIENT','-flto','-mno-target-align']
if a.shared_text_policy:flags+=['-DPORTABLE_TEXT_INPUT_POLICY']
repos={'system-apps':ROOT,'utilities':utilities,'runtime':runtime}
sources,includes,allowed=application_inputs('file_browser',ROOT/'Apps/file_browser.c',repos,watch,out,'runtime-features')
sources=[s for s in sources if s.name!='empty_catalog.c']
flags+=['-DPORTABLE_RUNTIME_APP_CATALOG','-DPORTABLE_NAVIGATION_RETENTION','-DPORTABLE_ASYNC_RGB565','-DPORTABLE_WATCH_NATIVE_HEIGHT']
includes += [utilities/'lib/Contexts/include',runtime/'sdk/app']
(out/'empty_catalog.c').write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
args=SimpleNamespace(peer_files=a.peer_files,webdav_sharing=True,sharing_export_instance=0,sharing_tcp_instance=0,sharing_entropy_instance=0,sharing_setup_instance=0)
sf,ss,proof=sharing.configure(args,ROOT,out,includes[0],cc);flags+=sf;sources+=ss
mapping=out/'exports.map';mapping.write_text('{global:app_main;app_module_init;app_module_fini;local:*;};\n')
elf=out/'file_browser.elf'
cmd=[cc,'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wl,--version-script='+str(mapping),'-Wall','-Wextra','-Werror',*flags,*['-I'+str(i) for i in includes],*map(str,sources),'-lgcc','-o',str(elf)]
subprocess.run(cmd,check=True)
syms=subprocess.check_output([cc.removesuffix('gcc')+'nm','-D',str(elf)],text=True)
imports={x.split()[-1] for x in syms.splitlines() if ' U ' in ' '+x}
exports={x.split()[-1] for x in syms.splitlines() if len(x.split())>=3 and x.split()[-2] in ('T','D','B','R')}
assert imports<=allowed|{'strstr','strchr','memchr'},imports-allowed
assert exports=={'app_main','app_module_init','app_module_fini'}
validator=out/'validate-elf'
subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'),str(ROOT/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
subprocess.run([str(validator),str(elf)],check=True)
compaction=None
if a.shared_text_input:
 from compact_current_elf import compact
 compaction=compact(elf,cc,debug_path=out/'debug/file_browser.elf')
 subprocess.run([str(validator),str(elf)],check=True)
sharing.requirements(args,manifest['requires'])
if {'capability':'app.catalog','api':1} not in manifest['requires']:manifest['requires'].append({'capability':'app.catalog','api':1})
if {'capability':'file.open','api':1} not in manifest['requires']:manifest['requires'].append({'capability':'file.open','api':1})
if a.source_handoff and {'capability':'file.open.source','api':1} not in manifest['requires']:manifest['requires'].append({'capability':'file.open.source','api':1})
if a.shared_text_input and {'capability':'ui.text-input','api':1} not in manifest['requires']:manifest['requires'].append({'capability':'ui.text-input','api':1})
manifest['version']='1.5.37' if a.shared_text_policy else '1.5.36' if a.source_handoff else '1.5.35' if a.shared_text_input else sharing.selected_version(a)
grants=old['grants'].copy()
if a.volume_instance is not None:
 q=dict(capability='storage.volume',api=1);g=dict(**q,instance_id=a.volume_instance)
 if q not in manifest['requires']:manifest['requires'].append(q)
 if g not in grants:grants.append(g)
if dict(capability='app.catalog',api=1,instance_id=0) not in grants:grants.append(dict(capability='app.catalog',api=1,instance_id=0))
for cap in ('input.power-key','power.sleep','display.power'):
 if dict(capability=cap,api=1,instance_id=0) not in grants:grants.append(dict(capability=cap,api=1,instance_id=0))
 if dict(capability=cap,api=1) not in manifest['requires']:manifest['requires'].append(dict(capability=cap,api=1))
for cap,api,instance in [('storage.app-data.export',1,0),('network.tcp.listener',1,0),('crypto.entropy',1,0),('bluetooth.session-setup',1,0),('storage.key-value',1,6),('file.open',1,0)]:
 g=dict(capability=cap,api=api,instance_id=instance)
 if g not in grants:grants.append(g)
if a.source_handoff and dict(capability='file.open.source',api=1,instance_id=0) not in grants:grants.append(dict(capability='file.open.source',api=1,instance_id=0))
if a.shared_text_input and dict(capability='ui.text-input',api=1,instance_id=0) not in grants:grants.append(dict(capability='ui.text-input',api=1,instance_id=0))
assert all(g in grants for g in old['grants'])
assert len(grants)<=32 and len(manifest['requires'])<=32
record={'schema':1,'compaction':compaction,'purpose':'target component; no firmware or hardware qualification','product_baseline':watch_commit,'version':manifest['version'],'required_grants':grants,'policy_rows':len(grants),'defines':flags,'imports':sorted(imports),'exports':sorted(exports),'bytes':elf.stat().st_size,'sha256':hashlib.sha256(elf.read_bytes()).hexdigest(),'sources':{n:subprocess.check_output(['git','-C',str(r),'rev-parse','HEAD'],text=True).strip() for n,r in {'system':ROOT,'runtime':runtime,'watch':watch,'utilities':utilities}.items()},'baseline_files':{n:hashlib.sha256((a.baseline/n).read_bytes()).hexdigest() for n in ('boot.json','file_browser.json')},'baseline_grants_preserved':True,'command':cmd,'composition_required':['current Runtime with 24 app policy rows; exact graph/live-grant capacity admission','explicit AppData export map with existing owner namespace identity','provider graph additions and complete 25-app store admission','file.open handler declarations and transport membership checks'],'source_sha256':{str(s):hashlib.sha256(s.read_bytes()).hexdigest() for s in sources if s.suffix!='.o'}}
# Hash all local headers/includes in the compiled source trees as well as C units.
for tree in (ROOT/'lib/PortableApps', ROOT/'Apps', ROOT/'lib/RemoteFiles',runtime/'sdk/app',utilities/'lib/Alarm/include'):
 for dep in tree.rglob('*'):
  if dep.is_file() and dep.suffix in ('.h','.inc','.c','.cpp'):
   record['source_sha256'][str(dep)]=hashlib.sha256(dep.read_bytes()).hexdigest()
(out/'file_browser.json').write_text(json.dumps(manifest,indent=2)+'\n')
(out/'file_browser-build-record.json').write_text(json.dumps(record,indent=2)+'\n')
print('Watch Files: full inherited app feature flags, existing authority preserved;',len(grants),'policy rows; target ELF validated')

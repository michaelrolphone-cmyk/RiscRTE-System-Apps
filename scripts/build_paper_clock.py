#!/usr/bin/env python3
"""Build the explicit wall-time Nova7 default.elf profile. No hardware access."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
import portable_quick_build
ROOT=Path(__file__).resolve().parents[1]
def build():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--display-rotation',type=int,choices=[0,90],default=90)
 p.add_argument('--navigation',action='store_true');p.add_argument('--alarm-client',action='store_true')
 p.add_argument('--launcher-app',default='springboard.elf');p.add_argument('--output-dir',type=Path,default=ROOT/'dist/paper-clock')
 p.add_argument("--local-sleep-source",type=Path,help="Explicit deployment sleep hook; requires navigation and alarm client")
 p.add_argument("--sleep-capability",help="Explicit power capability granted only to this clock")
 p.add_argument("--sleep-sdk",type=Path,help="Canonical deployment SDK for local sleep hook")
 portable_quick_build.options(p)
 a=p.parse_args()
 if bool(a.local_sleep_source)!=bool(a.sleep_capability) or (a.local_sleep_source and (not a.navigation or not a.alarm_client or not a.sleep_sdk)):p.error('Local sleep requires source, capability, SDK, navigation and alarm client')
 if not a.launcher_app.endswith('.elf') or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.' for c in a.launcher_app):p.error('Invalid launcher filename')
 cc=os.environ.get('NATIVE_APP_CC') or shutil.which('xtensa-esp32s3-elf-gcc') or str(Path(os.environ.get('PLATFORMIO_CORE_DIR',Path.home()/'.platformio'))/'packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc')
 out=a.output_dir;out.mkdir(parents=True,exist_ok=True)
 catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
 exports={'app_main','app_module_init','app_module_fini'};mapping=out/'exports.map';mapping.write_text('{ global: '+ '; '.join(sorted(exports))+'; local: *; };\n')
 flags=['-DPORTABLE_APP_OWNS_TOUCH_CHROME','-DPORTABLE_RTC_WALL_TIME','-DPORTABLE_DISPLAY_ROTATION='+str(a.display_rotation),'-DPAPER_CLOCK_LAUNCHER="'+a.launcher_app+'"']
 if a.navigation:flags+=['-DPORTABLE_INPUT_NAVIGATION']
 if a.navigation and not a.local_sleep_source:flags+=['-DPORTABLE_CROWN_SLEEP_UNAVAILABLE']
 if a.local_sleep_source:flags+=['-DPORTABLE_APP_SLEEP_LOCAL','-DPORTABLE_CROWN_SLEEP_LOCAL','-DPORTABLE_SLEEP_MANUAL_ONLY','-I'+str(a.sleep_sdk)]
 if a.alarm_client:flags+=['-DPORTABLE_ALARM_CLIENT']
 quick_flags,quick_sources=portable_quick_build.configure(a,p,ROOT,out);flags+=quick_flags
 elf=out/'default.elf'
 subprocess.run([cc,'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-Wl,--hash-style=sysv','-Wl,--version-script='+str(mapping),'-Wall','-Wextra','-Werror',*flags,'-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),str(ROOT/'Apps/paper_clock.c'),str(ROOT/'lib/PortableApps/src/adapter.c'),str(catalog),*map(str,quick_sources),*([str(a.local_sleep_source)] if a.local_sleep_source else []),'-lgcc','-o',str(elf)],check=True)
 symbols=subprocess.check_output([cc.removesuffix('gcc')+'nm','-D',str(elf)],text=True)
 imports={s.split()[-1] for s in symbols.splitlines() if ' U ' in ' '+s};actual={s.split()[-1] for s in symbols.splitlines() if len(s.split())>=3 and s.split()[-2] in ('T','D','B','R')}
 allowed={'risc_runtime_get_api','memcpy','memset','memcmp','strcmp','strlen','snprintf','strcpy','malloc','free'}
 if not imports<=allowed or actual!=exports:raise ValueError((imports-allowed,actual))
 validator=out/'validate-elf'
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'),str(ROOT/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
 subprocess.run([str(validator),str(elf)],check=True)
 version=json.loads((ROOT/'Apps/paper_clock.json').read_text())['version']
 if a.local_sleep_source:version='0.2.2'
 needs=[{'capability':n,'api':v} for n,v in [('display.output',1),('input.touch.raw',1),('rtc.clock',2),('board.battery',1),('storage.key-value',1)]]
 if a.navigation:needs.append({'capability':'input.navigation','api':1})
 if a.sleep_capability:needs.append({'capability':a.sleep_capability,'api':1})
 if a.alarm_client:needs.append({'capability':'alarm.service','api':1})
 portable_quick_build.requirements(a,needs)
 (out/'default.json').write_text(json.dumps({'type':'application','id':'paper_clock','version':version,'architecture':'xtensa-esp32s3','file_name':'default.elf','entry':'app_main','requires':needs},indent=2)+'\n')
 data=elf.read_bytes()
 record={'purpose':'development-artifact-no-hardware-qualification','version':version,'clock_policy':'rtc-wall-time','display_rotation':a.display_rotation,'launcher_app':a.launcher_app,'navigation':a.navigation,'sleep_capability':a.sleep_capability,'local_sleep_source_sha256':hashlib.sha256(a.local_sleep_source.read_bytes()).hexdigest() if a.local_sleep_source else None,'alarm_client':a.alarm_client,'quick_actions':a.quick_actions,'quick_radios':a.quick_radios,'home_app':a.home_app,'sha256':hashlib.sha256(data).hexdigest(),'size_bytes':len(data),'imports':sorted(imports),'compiler':subprocess.check_output([cc,'--version'],text=True).splitlines()[0],'repository_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'working_tree_dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True).strip())}
 (out/'build-evidence.json').write_text(json.dumps(record,indent=2)+'\n')
 for folder in ['fonts','paper_fonts']:
  target=out/'licenses'/folder;target.mkdir(parents=True,exist_ok=True)
  for path in (ROOT/'lib/PortableApps'/folder).glob('LICENSE*'):shutil.copyfile(path,target/path.name)
 print('Paper default clock: target structural validation and imports/exports passed')
if __name__=='__main__':build()

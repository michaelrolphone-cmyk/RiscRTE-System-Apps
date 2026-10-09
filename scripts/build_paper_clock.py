#!/usr/bin/env python3
"""Build the explicit wall-time Nova7 default.elf profile. No hardware access."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
import portable_quick_build
import portable_alarm_build
ROOT=Path(__file__).resolve().parents[1]
def build():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--display-rotation',type=int,choices=[0,90],default=90)
 p.add_argument('--navigation',action='store_true');p.add_argument('--alarm-client',action='store_true')
 p.add_argument('--launcher-app',default='springboard.elf');p.add_argument('--output-dir',type=Path,default=ROOT/'dist/paper-clock')
 p.add_argument("--local-sleep-source",type=Path,help="Explicit deployment sleep hook; requires navigation and alarm client")
 p.add_argument("--sleep-capability",help="Explicit power capability granted only to this clock")
 p.add_argument("--sleep-sdk",type=Path,help="Canonical deployment SDK for local sleep hook")
 p.add_argument("--desk-clock",action="store_true",help="Opt-in retained six-face wall-time desk clock; requires local sleep and canonical SDKs")
 p.add_argument("--sparse-start",action="store_true",help="Opt-in native-time demand Clock; requires desk clock and canonical invocation-retention Runtime SDK")
 p.add_argument("--retained-wake-sdk",type=Path,help="Canonical Runtime app SDK containing RiscRetainedWakeV1.h")
 portable_quick_build.options(p)
 portable_alarm_build.options(p)
 a=p.parse_args()
 if bool(a.local_sleep_source)!=bool(a.sleep_capability) or (a.local_sleep_source and (not a.navigation or not a.alarm_client or not a.sleep_sdk)):p.error('Local sleep requires source, capability, SDK, navigation and alarm client')
 if a.desk_clock and (not a.local_sleep_source or not a.retained_wake_sdk):p.error('Desk clock requires local sleep and retained-wake SDK')
 if a.desk_clock and not (a.retained_wake_sdk/'RiscRetainedWakeV1.h').is_file():p.error('Missing canonical RiscRetainedWakeV1.h')
 if a.tagged_alarm_utilities and not a.sparse_start:p.error('Tagged alarm Clock requires the native sparse profile')
 if a.sparse_start:
  if not a.desk_clock:p.error('Sparse startup requires --desk-clock')
  if a.sleep_capability!='x4.power':p.error('Sparse startup requires the X4 x4.power contract')
  for name in ('RiscRuntimeV1.h','RiscRealtimeV1.h','RiscProviderPromotionV1.h'):
   if not (a.retained_wake_sdk/name).is_file():p.error('Missing canonical '+name)
  if 'RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE' not in (a.retained_wake_sdk/'RiscRuntimeV1.h').read_text():p.error('Sparse startup requires the canonical invocation-retention Runtime suffix')
 if not a.launcher_app.endswith('.elf') or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.' for c in a.launcher_app):p.error('Invalid launcher filename')
 cc=os.environ.get('NATIVE_APP_CC') or shutil.which('xtensa-esp32s3-elf-gcc') or str(Path(os.environ.get('PLATFORMIO_CORE_DIR',Path.home()/'.platformio'))/'packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc')
 out=a.output_dir;out.mkdir(parents=True,exist_ok=True)
 includes=ROOT/'lib/PortableApps/include'
 if a.desk_clock:
  # Stage canonical extensions with their exact prefixes. Mixing a copied old
  # prefix and canonical suffix by include-path order would redefine C types.
  includes=out/'desk-sdk/include';includes.mkdir(parents=True,exist_ok=True)
  for header in (ROOT/'lib/PortableApps/include').glob('*.h'):shutil.copyfile(header,includes/header.name)
  time_dir=includes.parent/'time'
  shutil.copytree(ROOT/'lib/PortableApps/time',time_dir,dirs_exist_ok=True)
  for name in ('RiscDisplayOutputV1.h','RiscDisplayOutputPowerV1.h','RiscTouchV1.h','RiscTouchPowerV1.h','RiscStorageVolumeV1.h'):
   shutil.copyfile(a.sleep_sdk/name,includes/name)
  shutil.copyfile(a.retained_wake_sdk/'RiscRetainedWakeV1.h',includes/'RiscRetainedWakeV1.h')
  for name in ('RiscTimedSleepV1.h','RiscLightSleepV1.h','RiscDeepSleepV1.h'):shutil.copyfile(a.retained_wake_sdk.parent/'driver'/name,includes/name)
  if a.sparse_start:
   for name in ('RiscRuntimeV1.h','RiscRealtimeV1.h','RiscProviderPromotionV1.h'):
    shutil.copyfile(a.retained_wake_sdk/name,includes/name)

 tagged_alarm=portable_alarm_build.stage(a,p,out,includes)
 catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
 exports={'app_main','app_module_init','app_module_fini'};mapping=out/'exports.map';mapping.write_text('{ global: '+ '; '.join(sorted(exports))+'; local: *; };\n')
 flags=['-DPORTABLE_APP_OWNS_TOUCH_CHROME','-DPORTABLE_RTC_WALL_TIME','-DPORTABLE_DISPLAY_ROTATION='+str(a.display_rotation),'-DPAPER_CLOCK_LAUNCHER="'+a.launcher_app+'"']
 if a.navigation:flags+=['-DPORTABLE_INPUT_NAVIGATION']
 if a.navigation and not a.local_sleep_source:flags+=['-DPORTABLE_CROWN_SLEEP_UNAVAILABLE']
 if a.local_sleep_source:flags+=['-DPORTABLE_APP_SLEEP_LOCAL','-DPORTABLE_CROWN_SLEEP_LOCAL','-DPORTABLE_SLEEP_MANUAL_ONLY','-I'+str(a.sleep_sdk)]
 if a.desk_clock:
  flags=[flag for flag in flags if flag!='-I'+str(a.sleep_sdk)]
  flags+=['-DPORTABLE_DESK_CLOCK']
 if a.sparse_start:flags+=['-DPORTABLE_DESK_CLOCK_SPARSE_START']
 if a.alarm_client:flags+=['-DPORTABLE_ALARM_CLIENT']
 if tagged_alarm:flags+=['-DALARM_SERVICE_TAGGED_V2']
 quick_flags,quick_sources=portable_quick_build.configure(a,p,ROOT,out);flags+=quick_flags
 sparse_sources=[ROOT/'lib/PortableApps/src'/name for name in ('PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c')] if a.sparse_start else []
 elf=out/'default.elf'
 subprocess.run([cc,'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-Wl,--hash-style=sysv','-Wl,--version-script='+str(mapping),'-Wall','-Wextra','-Werror',*flags,'-I'+str(includes),'-I'+str(ROOT/'lib/NativeApps/include'),str(ROOT/'Apps/paper_clock.c'),str(ROOT/'lib/PortableApps/src/adapter.c'),str(catalog),*map(str,quick_sources),*([str(ROOT/'lib/PortableApps/src/desk_clock_faces.c')] if a.desk_clock else []),*([str(a.local_sleep_source)] if a.local_sleep_source else []),*map(str,sparse_sources),'-lgcc','-o',str(elf)],check=True)
 symbols=subprocess.check_output([cc.removesuffix('gcc')+'nm','-D',str(elf)],text=True)
 imports={s.split()[-1] for s in symbols.splitlines() if ' U ' in ' '+s};actual={s.split()[-1] for s in symbols.splitlines() if len(s.split())>=3 and s.split()[-2] in ('T','D','B','R')}
 allowed={'risc_runtime_get_api','memcpy','memset','memcmp','strcmp','strlen','snprintf','strcpy','malloc','free'}
 if not imports<=allowed or actual!=exports:raise ValueError((imports-allowed,actual))
 validator=out/'validate-elf'
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'),str(ROOT/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
 subprocess.run([str(validator),str(elf)],check=True)
 version=json.loads((ROOT/'Apps/paper_clock.json').read_text())['version']
 if a.local_sleep_source:version='0.2.2'
 if a.desk_clock:version='0.3.1'
 if a.sparse_start:version='0.3.5' if tagged_alarm else '0.3.3'
 needs=[{'capability':n,'api':v} for n,v in [('display.output',1),('input.touch.raw',1),('rtc.clock',2),('board.battery',1),('storage.key-value',1)]]
 if a.navigation:needs.append({'capability':'input.navigation','api':1})
 if a.sleep_capability:needs.append({'capability':a.sleep_capability,'api':1})
 if a.desk_clock:needs.extend([{'capability':'runtime.retained-wake','api':1},{'capability':'storage.volume','api':1}])
 if a.alarm_client:needs.append({'capability':'alarm.service','api':2 if tagged_alarm else 1})
 portable_quick_build.requirements(a,needs)
 if a.desk_clock:
  for name in ('net.wifi','bluetooth.hci'):
   requirement={'capability':name,'api':1}
   if requirement not in needs:needs.append(requirement)
  if a.sparse_start:needs.extend([{'capability':'runtime.realtime-control','api':1},{'capability':'runtime.provider-promotion','api':1}])
  if len(needs)!=(14 if a.sparse_start else 12):raise ValueError('Desk clock grant count mismatch')
 (out/'default.json').write_text(json.dumps({'type':'application','id':'paper_clock','version':version,'architecture':'xtensa-esp32s3','file_name':'default.elf','entry':'app_main','requires':needs},indent=2)+'\n')
 data=elf.read_bytes()
 record={'purpose':'development-artifact-no-hardware-qualification','desk_clock':a.desk_clock,'retained_wake_sdk_sha256':hashlib.sha256((a.retained_wake_sdk/'RiscRetainedWakeV1.h').read_bytes()).hexdigest() if a.desk_clock else None,'version':version,'clock_policy':'rtc-wall-time','display_rotation':a.display_rotation,'launcher_app':a.launcher_app,'navigation':a.navigation,'sleep_capability':a.sleep_capability,'local_sleep_source_sha256':hashlib.sha256(a.local_sleep_source.read_bytes()).hexdigest() if a.local_sleep_source else None,'alarm_client':a.alarm_client,'quick_actions':a.quick_actions,'quick_radios':a.quick_radios,'home_app':a.home_app,'sha256':hashlib.sha256(data).hexdigest(),'size_bytes':len(data),'imports':sorted(imports),'compiler':subprocess.check_output([cc,'--version'],text=True).splitlines()[0],'repository_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'working_tree_dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True).strip())}
 if tagged_alarm:record['tagged_alarm_sdk']=tagged_alarm
 if a.desk_clock:
  record['desk_sdk_headers']={name:hashlib.sha256((includes/name).read_bytes()).hexdigest() for name in (
   'RiscDisplayOutputV1.h','RiscDisplayOutputPowerV1.h','RiscTouchV1.h','RiscTouchPowerV1.h',
   'RiscStorageVolumeV1.h','RiscRetainedWakeV1.h','RiscTimedSleepV1.h','RiscLightSleepV1.h','RiscDeepSleepV1.h')}
  paths=['Apps/paper_clock.c','Apps/paper_desk_clock.inc','lib/PortableApps/src/adapter.c',
   'lib/PortableApps/src/desk_clock_faces.c','lib/PortableApps/include/PortableDeskClockApp.h',
   'lib/PortableApps/include/PortableDeskClock.h','lib/PortableApps/include/PortableDeskClockSettings.h',
   'lib/PortableApps/include/PortableSleepPolicy.h','lib/PortableApps/include/PortableReaderPreferences.h','lib/PortableApps/src/paper.inc','scripts/build_paper_clock.py']
  paths.extend(str(path.relative_to(ROOT)) for path in quick_sources)
  if tagged_alarm:paths.append('scripts/portable_alarm_build.py')
  record['desk_sources']={path:hashlib.sha256((ROOT/path).read_bytes()).hexdigest() for path in paths}
  record['time_resolution']='whole-second RTC, <=100ms observed edge bracket; monotonic deadline; native timer-arm latency unqualified'
  record['grant_count']=len(needs)
  if a.sparse_start:
   record.update(clock_policy='native-realtime-iana',sparse_start=True,provider_activation='demand',
                 timer_preferences='retained-only',foreground_promotion=True,invocation_retention=True,
                 time_resolution='native microsecond snapshot before holds; bounded same-boot projection after holds; physical accuracy and entry latency unqualified')
   record['desk_sdk_headers'].update({name:hashlib.sha256((includes/name).read_bytes()).hexdigest() for name in ('RiscRuntimeV1.h','RiscRealtimeV1.h','RiscProviderPromotionV1.h')})
   paths=['lib/PortableApps/boot_logo/RiscRteLogo.h','lib/PortableApps/boot_logo/logo.svg','lib/PortableApps/boot_logo/SOURCES.json','Apps/paper_sparse_clock.inc','lib/PortableApps/src/sparse_clock_adapter.inc','lib/PortableApps/src/alarm.inc','lib/PortableApps/src/nova.inc','lib/PortableApps/src/quick_adapter.inc',
          'lib/PortableApps/include/PortableRealtimeClient.h','lib/PortableApps/include/PortableTimeZone.h',
          'lib/PortableApps/include/PortableTimeZonePreference.h','lib/PortableApps/include/PortableRtcBasis.h']
   paths.extend(str(path.relative_to(ROOT)) for path in sparse_sources)
   record['desk_sources'].update({path:hashlib.sha256((ROOT/path).read_bytes()).hexdigest() for path in paths})
 (out/'build-evidence.json').write_text(json.dumps(record,indent=2)+'\n')
 for folder in ['fonts','paper_fonts']+(['desk_clock'] if a.desk_clock else []):
  target=out/'licenses'/folder;target.mkdir(parents=True,exist_ok=True)
  for path in (ROOT/'lib/PortableApps'/folder).glob('LICENSE*'):shutil.copyfile(path,target/path.name)
  if folder=='desk_clock':shutil.copyfile(ROOT/'lib/PortableApps/desk_clock/SOURCES.json',target/'SOURCES.json')
 if a.sparse_start:shutil.copytree(ROOT/'lib/PortableApps/boot_logo',out/'licenses/boot_logo',dirs_exist_ok=True)
 print('Paper default clock: target structural validation and imports/exports passed')
if __name__=='__main__':build()

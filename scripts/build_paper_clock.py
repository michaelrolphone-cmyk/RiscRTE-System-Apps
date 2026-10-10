#!/usr/bin/env python3
"""Build the explicit wall-time Nova7 default.elf profile. No hardware access."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
import portable_quick_build
import portable_idle_build
import portable_alarm_build
import portable_performance_build
import portable_paper_build
import portable_loading_build
ROOT=Path(__file__).resolve().parents[1]
def build():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--display-rotation',type=int,choices=[0,90],default=90)
 p.add_argument('--navigation',action='store_true');p.add_argument('--alarm-client',action='store_true')
 p.add_argument('--launcher-app',default='springboard.elf');p.add_argument('--output-dir',type=Path,default=ROOT/'dist/paper-clock')
 p.add_argument('--resident-loading-catalog',type=Path,help='Existing deployment catalog used by the shared host loading screen')
 p.add_argument("--local-sleep-source",type=Path,help="Explicit deployment sleep hook; requires navigation and alarm client")
 p.add_argument("--sleep-capability",help="Explicit power capability granted only to this clock")
 p.add_argument("--sleep-sdk",type=Path,help="Canonical deployment SDK for local sleep hook")
 p.add_argument("--ble-broadcast",action="store_true",help="Select native transient BLE telemetry client, off by default")
 p.add_argument("--contexts-rf-only",action="store_true",help="Explicit RF-only Contexts monitoring and owner-model rendezvous in default Clock")
 p.add_argument("--wake-light-restore",action="store_true",help="Restore the persisted frontlight only after a sparse deep GPIO wake")
 p.add_argument("--desk-points-face",action="store_true",help="Retained catalog Points desk face")
 p.add_argument("--desk-lock-home",action="store_true",help="X4 Home crown locks the landscape deep desk clock; release/repress wakes")
 p.add_argument("--desk-clock",action="store_true",help="Opt-in retained six-face wall-time desk clock; requires local sleep and canonical SDKs")
 p.add_argument("--sparse-start",action="store_true",help="Opt-in native-time demand Clock; requires desk clock and canonical invocation-retention Runtime SDK")
 p.add_argument("--retained-wake-sdk",type=Path,help="Canonical Runtime app SDK containing RiscRetainedWakeV1.h")
 portable_quick_build.options(p)
 portable_alarm_build.options(p)
 portable_performance_build.options(p)
 portable_paper_build.options(p)
 a=p.parse_args()
 if a.resident_loading_catalog and not a.resident_shell_host:p.error('--resident-loading-catalog requires the resident host')
 portable_performance_build.validate(a,p)
 portable_paper_build.validate(a,p)
 if portable_performance_build.selected(a) and not a.sparse_start:p.error('--performance-runtime-repo requires --sparse-start')
 if bool(a.local_sleep_source)!=bool(a.sleep_capability) or (a.local_sleep_source and (not a.navigation or not a.alarm_client or not a.sleep_sdk)):p.error('Local sleep requires source, capability, SDK, navigation and alarm client')
 if a.desk_clock and (not a.local_sleep_source or not a.retained_wake_sdk):p.error('Desk clock requires local sleep and retained-wake SDK')
 if a.desk_clock and not (a.retained_wake_sdk/'RiscRetainedWakeV1.h').is_file():p.error('Missing canonical RiscRetainedWakeV1.h')
 if a.tagged_alarm_utilities and not a.sparse_start:p.error('Tagged alarm Clock requires the native sparse profile')
 if a.ble_broadcast and not a.sparse_start:p.error("--ble-broadcast requires --sparse-start")
 if a.desk_points_face and not (a.desk_lock_home and a.tagged_alarm_utilities):p.error('--desk-points-face requires --desk-lock-home and --tagged-alarm-utilities')
 if a.desk_points_face and (not a.retained_wake_sdk or 'RISC_RETAINED_WAKE_EXTENDED_TAG' not in (a.retained_wake_sdk/'RiscRetainedWakeV1.h').read_text()):p.error('Catalog Points requires the canonical extended retained API')
 if a.desk_lock_home and not a.sparse_start:p.error('--desk-lock-home requires --sparse-start')
 if a.wake_light_restore and not (a.sparse_start and a.quick_actions):p.error("--wake-light-restore requires sparse start and Quick Actions host support")
 if a.contexts_rf_only and (not a.sparse_start or not a.quick_actions or not portable_idle_build.selected(a) or not a.tagged_alarm_utilities):
  p.error("--contexts-rf-only requires sparse Clock, Quick Actions, tagged alarm and checked X4 idle profile")
 if a.sparse_start:
  if not a.desk_clock:p.error('Sparse startup requires --desk-clock')
  if a.sleep_capability!='x4.power':p.error('Sparse startup requires the X4 x4.power contract')
  for name in ('RiscRuntimeV1.h','RiscRealtimeV1.h','RiscProviderPromotionV1.h'):
   if not (a.retained_wake_sdk/name).is_file():p.error('Missing canonical '+name)
  if 'RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE' not in (a.retained_wake_sdk/'RiscRuntimeV1.h').read_text():p.error('Sparse startup requires the canonical invocation-retention Runtime suffix')
 if not a.launcher_app.endswith('.elf') or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.' for c in a.launcher_app):p.error('Invalid launcher filename')
 performance_source=portable_performance_build.read(a,p) if portable_performance_build.selected(a) else None
 performance=None
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
 if performance_source:includes,performance=portable_performance_build.stage(ROOT,out,performance_source,includes,
  display=portable_performance_build.read_display(a,p),overrides={'RiscStorageVolumeV1.h':a.sleep_sdk/'RiscStorageVolumeV1.h'})

 includes,paper_flags,paper_transition=portable_paper_build.stage(a,p,ROOT,out,includes)
 tagged_alarm=portable_alarm_build.stage(a,p,out,includes)
 loading=None
 catalog=out/'catalog.c'
 if a.resident_loading_catalog:
  catalog_text,loading=portable_loading_build.source(ROOT,a.resident_loading_catalog,a.launcher_app)
  catalog.write_text(catalog_text)
 else:catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
 exports=portable_quick_build.exports(a,{'app_main','app_module_init','app_module_fini'});mapping=out/'exports.map';mapping.write_text('{ global: '+ '; '.join(sorted(exports))+'; local: *; };\n')
 flags=['-DPORTABLE_APP_OWNS_TOUCH_CHROME','-DPORTABLE_RTC_WALL_TIME','-DPORTABLE_DISPLAY_ROTATION='+str(a.display_rotation),'-DPAPER_CLOCK_LAUNCHER="'+a.launcher_app+'"']
 flags+=paper_flags
 if a.ble_broadcast:flags += ["-DPORTABLE_BLE_BROADCAST","-DPORTABLE_BLE_BROADCAST_DEFAULT_OFF"]
 if a.desk_points_face:flags+=['-DPORTABLE_DESK_POINTS_FACE','-DPORTABLE_DESK_POINTS_SNAPSHOT']
 if a.desk_lock_home:flags+=['-DPORTABLE_DESK_LOCK_HOME']
 if a.navigation:flags+=['-DPORTABLE_INPUT_NAVIGATION']
 if a.navigation and not a.local_sleep_source:flags+=['-DPORTABLE_CROWN_SLEEP_UNAVAILABLE']
 if a.local_sleep_source:flags+=['-DPORTABLE_APP_SLEEP_LOCAL','-DPORTABLE_CROWN_SLEEP_LOCAL',*([] if portable_idle_build.selected(a) else ['-DPORTABLE_SLEEP_MANUAL_ONLY']),'-I'+str(a.sleep_sdk)]
 if a.desk_clock:
  flags=[flag for flag in flags if flag!='-I'+str(a.sleep_sdk)]
  flags+=['-DPORTABLE_DESK_CLOCK']
 if a.sparse_start:flags+=['-DPORTABLE_DESK_CLOCK_SPARSE_START']
 if a.contexts_rf_only:flags+=['-DPORTABLE_CONTEXTS_CLIENT','-DPORTABLE_CONTEXTS_CLOCK_RF_ONLY']
 if a.wake_light_restore:flags+=['-DPORTABLE_DESK_WAKE_LIGHT']
 if performance:flags.extend(portable_performance_build.defines(performance,getattr(a,'stage_logs',False),out))
 if a.alarm_client:flags+=['-DPORTABLE_ALARM_CLIENT']
 if tagged_alarm:flags+=['-DALARM_SERVICE_TAGGED_V2','-DPORTABLE_HOME_POINTS_NATIVE_UTC','-DALARM_NATIVE_UTC']
 quick_flags,quick_sources=portable_quick_build.configure(a,p,ROOT,out,includes);flags+=quick_flags
 if loading:
  if 'RISC_RESIDENT_CALLBACKS_LOADING_V1_SIZE' not in (includes/'RiscResidentShellV1.h').read_text():p.error('Loading screen requires the canonical Runtime loading callback SDK')
  flags+=['-DPORTABLE_RESIDENT_LOADING']
 sparse_sources=[ROOT/'lib/PortableApps/src'/name for name in ('PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c')] if a.sparse_start else []
 elf=out/'default.elf'
 subprocess.run([cc,'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-Wl,--hash-style=sysv','-Wl,--version-script='+str(mapping),'-Wall','-Wextra','-Werror',*flags,'-I'+str(includes),'-I'+str(ROOT/'lib/NativeApps/include'),str(ROOT/'Apps/paper_clock.c'),str(ROOT/'lib/PortableApps/src/adapter.c'),str(catalog),*map(str,quick_sources),*([str(ROOT/'lib/PortableApps/src/desk_clock_faces.c')] if a.desk_clock else []),*([str(a.local_sleep_source)] if a.local_sleep_source else []),*map(str,sparse_sources),'-lgcc','-o',str(elf)],check=True)
 symbols=subprocess.check_output([cc.removesuffix('gcc')+'nm','-D',str(elf)],text=True)
 imports={s.split()[-1] for s in symbols.splitlines() if ' U ' in ' '+s};actual={s.split()[-1] for s in symbols.splitlines() if len(s.split())>=3 and s.split()[-2] in ('T','D','B','R')}
 allowed={'risc_runtime_get_api','memcpy','memmove','memset','memcmp','strcmp','strlen','snprintf','strcpy','malloc','free'}
 if a.contexts_rf_only:allowed|={"strncmp","memchr"}
 if not imports<=allowed or actual!=exports:raise ValueError((imports-allowed,actual))
 validator=out/'validate-elf'
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'),str(ROOT/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
 subprocess.run([str(validator),str(elf)],check=True)
 version=json.loads((ROOT/'Apps/paper_clock.json').read_text())['version']
 if a.local_sleep_source:version='0.2.2'
 if a.desk_clock:version='0.3.1'
 if a.sparse_start:version='0.3.6' if tagged_alarm else '0.3.3'
 if performance:version=portable_performance_build.version('paper_clock',a)
 if paper_transition or a.paper_transitions:version=portable_paper_build.VERSIONS['paper_clock']
 if a.desk_lock_home:version='0.3.10'
 if a.ble_broadcast:version='0.3.11'
 version=portable_idle_build.version(a,'paper_clock',version)
 if a.desk_points_face:version='0.3.17'
 version=portable_quick_build.version(a,'paper_clock',version)
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
  if a.ble_broadcast:needs.append({"capability":"telemetry.broadcast","api":1})
  if a.contexts_rf_only:needs.append({'capability':'contexts.service','api':1})
  if len(needs)!=((((15 if a.ble_broadcast else 14)+int(a.contexts_rf_only)) if a.sparse_start else 12)+int(a.crash_report_sd)):raise ValueError('Desk clock grant count mismatch')
 (out/'default.json').write_text(json.dumps({'type':'application','id':'paper_clock','version':version,'architecture':'xtensa-esp32s3','file_name':'default.elf','entry':'app_main','requires':needs},indent=2)+'\n')
 data=elf.read_bytes()
 record={'purpose':'development-artifact-no-hardware-qualification','desk_clock':a.desk_clock,'retained_wake_sdk_sha256':hashlib.sha256((a.retained_wake_sdk/'RiscRetainedWakeV1.h').read_bytes()).hexdigest() if a.desk_clock else None,'version':version,'clock_policy':'rtc-wall-time','display_rotation':a.display_rotation,'launcher_app':a.launcher_app,'navigation':a.navigation,'sleep_capability':a.sleep_capability,'local_sleep_source_sha256':hashlib.sha256(a.local_sleep_source.read_bytes()).hexdigest() if a.local_sleep_source else None,'alarm_client':a.alarm_client,'quick_actions':a.quick_actions,'quick_radios':a.quick_radios,'home_app':a.home_app,'sha256':hashlib.sha256(data).hexdigest(),'size_bytes':len(data),'imports':sorted(imports),'compiler':subprocess.check_output([cc,'--version'],text=True).splitlines()[0],'repository_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'working_tree_dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True).strip())}
 if a.ble_broadcast:record['ble_broadcast']={'enabled':True,'default':'off','grant_lifetime':'transient','timer_only':False,'shared_preferences_instance':1};record['build_defines']=flags
 if a.contexts_rf_only:
  record['contexts']={'profile':'rf-only','supported_sources':2,'owner':'waterfall.elf','service_api':1,
   'service_instance':0,'preferences_instance':1,'timer_only':False,'model_storage':'owner-export-only',
   'required_policy_rows':len(needs)+1,'required_capability_count':len(needs),
   'runtime_live_grant_limit':16,'runtime_requirement_limit':17 if a.crash_report_sd else 16,
   'source_sha256':{path:hashlib.sha256((ROOT/path).read_bytes()).hexdigest() for path in (
    'lib/PortableApps/include/ContextsServiceV1.h','lib/PortableApps/include/PortableContextsClient.h',
    'lib/PortableApps/include/PortableContextPreferences.h','lib/PortableApps/include/PortableBackgroundServices.h',
    'lib/PortableApps/include/PortableBroadcastAppData.h','lib/PortableApps/src/native_custody_adapter.inc',
    'lib/PortableApps/src/contexts_adapter.inc',
    'lib/PortableApps/src/contexts_policy.inc','lib/PortableApps/src/contexts_clock.inc')}}
  record['build_defines']=flags
 if a.ble_broadcast:
  record['ble_broadcast']['source_sha256']={path:hashlib.sha256((ROOT/path).read_bytes()).hexdigest() for path in (
   'lib/PortableApps/include/PortableBroadcastClient.h','lib/PortableApps/include/PortableBroadcastAppData.h',
   'lib/PortableApps/include/TelemetryBroadcastV1.h','lib/PortableApps/include/RiscTelemetryV1.h',
   'lib/PortableApps/include/RiscBluetoothTelemetryV1.h','lib/PortableApps/include/PortableRadioPolicy.h',
   'lib/PortableApps/src/broadcast_adapter.inc','lib/PortableApps/src/adapter.c','lib/PortableApps/src/alarm.inc',
   'lib/PortableApps/src/quick_adapter.inc','lib/PortableApps/src/sparse_clock_adapter.inc')}
 if paper_transition:record['paper_transition']=paper_transition
 if a.paper_transitions:record['paper_motion']=portable_paper_build.motion_receipt(ROOT)
 if paper_transition or a.paper_transitions:record['build_defines']=flags
 if performance:
  record['performance_trace']=performance
  record['build_defines']=flags
  record['retained_wake_sdk_sha256']=performance['sdk_headers']['RiscRetainedWakeV1.h']
 if tagged_alarm:
  record['tagged_alarm_sdk']=tagged_alarm
  record['home_points']={'clock_policy':'native-utc','storage_instance':5,'foreground_only':True,'records':['points_utc_cfg','points_utc_meta'],'projection':'Utilities PointsUtcSchedule','model':'Watch nova_points_state','tap_app':'points_in_time.elf'}
 if a.desk_points_face:
  record['home_points']={'clock_policy':'native-utc','foreground_only':True,'projection':'alarm.service copied catalog window','model':'PortablePointsCatalogView','tap_app':'points_in_time.elf','next_display_capacity':4,'label_bytes':32,'catalog_capacity':'filesystem-defined',
   'readiness_poll_ms':250,'readiness_phase':'settled display only; copied projection, no service pump or storage',
   'readiness_repaint':'status or visible catalog change; snapshot-only churn does not repaint','pending_label':'LOADING POINTS'}
  record['desk_points']={'face':6,'default_face':6,'name':'Clock+Points','retained_schema':3,'retained_bytes':408,'timer_projection':'retained window; admitted alarm refresh at expiry or rewind','timer_refresh_phase_budget':64,'timer_refresh_promotes':False,'provenance':'Home0.3.16 layout reconstructed; new catalog projection and native NOVA typography'}
  record['build_defines']=flags
 if a.desk_clock:
  record['desk_sdk_headers']={name:hashlib.sha256((includes/name).read_bytes()).hexdigest() for name in (
   'RiscDisplayOutputV1.h','RiscDisplayOutputPowerV1.h','RiscTouchV1.h','RiscTouchPowerV1.h',
   'RiscStorageVolumeV1.h','RiscRetainedWakeV1.h','RiscTimedSleepV1.h','RiscLightSleepV1.h','RiscDeepSleepV1.h')}
  paths=['Apps/paper_clock.c','Apps/PaperFrame.h','Apps/paper_desk_clock.inc','lib/PortableApps/src/adapter.c',
   'lib/PortableApps/src/desk_clock_faces.c','lib/PortableApps/include/PortableDeskClockApp.h',
   'lib/PortableApps/include/PortableDeskClock.h','lib/PortableApps/include/PortableDeskClockSettings.h',
   'lib/PortableApps/include/PortableSleepPolicy.h','lib/PortableApps/include/PortableReaderPreferences.h','lib/PortableApps/src/paper.inc','scripts/build_paper_clock.py']
  if a.ble_broadcast:paths.extend(['lib/PortableApps/include/PortableBroadcastClient.h','lib/PortableApps/include/TelemetryBroadcastV1.h','lib/PortableApps/src/broadcast_adapter.inc'])
  paths.extend(['lib/PortableApps/include/PortablePerformance.h','lib/PortableApps/src/performance.inc'])
  paths.extend(str(path.relative_to(ROOT)) for path in quick_sources if path.is_relative_to(ROOT))
  if performance:paths.append('scripts/portable_performance_build.py')
  if tagged_alarm:paths.extend(['scripts/portable_alarm_build.py','Apps/paper_home_points.inc','Apps/PaperHomePoints.h','Apps/PaperQuickIntent.h','Apps/paper_home_type.inc','lib/PortableApps/home_fonts/text.inc','lib/PortableApps/home_fonts/dial.inc','lib/PortableApps/home_fonts/SOURCES.json','lib/PortableApps/home_fonts/DIAL.json','lib/PortableApps/include/PortablePointsState.h'])
  if tagged_alarm:paths.extend(['Apps/paper_home_reference.inc','scripts/generate_home_reference_assets.py',*['lib/PortableApps/home_fonts/'+name for name in ['reference.inc','REFERENCE.json','Orbitron-reference-700.ttf','Orbitron-reference-900.ttf','reference-files.svg','reference-points.svg','reference-contexts.svg','reference-settings.svg']]])
  if a.desk_points_face:paths.extend(['Apps/PaperHomeCatalogPoints.h','lib/PortableApps/include/PortablePointsCatalogView.h','lib/PortableApps/include/PortableHomePointsCatalog.h','lib/PortableApps/include/PortableDeskPointsSnapshot.h','lib/PortableApps/home_fonts/desk_time.inc','lib/PortableApps/home_fonts/DESK_TIME.json','scripts/generate_home_desk_time.py'])
  if a.contexts_rf_only:paths.extend(record['contexts']['source_sha256'])
  record['desk_sources']={path:hashlib.sha256((ROOT/path).read_bytes()).hexdigest() for path in paths}
  if a.desk_points_face:record['desk_sdk_headers'].update({name:hashlib.sha256((includes/name).read_bytes()).hexdigest() for name in portable_alarm_build.CATALOG_HEADERS})
  if paper_transition:record['desk_sdk_headers']['RiscDisplayOutputSnapshotV1.h']=hashlib.sha256((includes/'RiscDisplayOutputSnapshotV1.h').read_bytes()).hexdigest()
  record['time_resolution']='whole-second RTC, <=100ms observed edge bracket; monotonic deadline; native timer-arm latency unqualified'
  record['grant_count']=len(needs)
  if a.sparse_start:
   record.update(clock_policy='native-realtime-iana',sparse_start=True,provider_activation='demand',
                 timer_preferences='retained-only',foreground_promotion=True,invocation_retention=True,
                 time_resolution='native microsecond snapshot before holds; bounded same-boot projection after holds; physical accuracy and entry latency unqualified')
   record['desk_sdk_headers'].update({name:hashlib.sha256((includes/name).read_bytes()).hexdigest() for name in ('RiscRuntimeV1.h','RiscRealtimeV1.h','RiscProviderPromotionV1.h')})
   paths=['lib/PortableApps/boot_logo/RiscRteLogo.h','lib/PortableApps/boot_logo/logo.svg','lib/PortableApps/boot_logo/SOURCES.json','Apps/paper_sparse_clock.inc','lib/PortableApps/src/sparse_clock_adapter.inc','lib/PortableApps/src/home_points_catalog_adapter.inc','lib/PortableApps/src/alarm.inc','lib/PortableApps/src/nova.inc','lib/PortableApps/src/quick_adapter.inc',
          'lib/PortableApps/include/PortableRealtimeClient.h','lib/PortableApps/include/PortableTimeZone.h',
          'lib/PortableApps/include/PortableTimeZonePreference.h','lib/PortableApps/include/PortableRtcBasis.h']
   paths.extend(str(path.relative_to(ROOT)) for path in sparse_sources)
   record['desk_sources'].update({path:hashlib.sha256((ROOT/path).read_bytes()).hexdigest() for path in paths})
 portable_idle_build.record(a,record,flags)
 if loading:record['app_loading']=loading;record['build_defines']=flags
 portable_quick_build.record(a,record)
 (out/'build-evidence.json').write_text(json.dumps(record,indent=2)+'\n')
 for folder in ['fonts','paper_fonts']+(['desk_clock'] if a.desk_clock else [])+(['home_fonts'] if tagged_alarm else []):
  target=out/'licenses'/folder;target.mkdir(parents=True,exist_ok=True)
  for path in (ROOT/'lib/PortableApps'/folder).glob('LICENSE*'):shutil.copyfile(path,target/path.name)
  if folder=='home_fonts':shutil.copyfile(ROOT/'lib/PortableApps/home_fonts/REFERENCE.json',target/'REFERENCE.json')
  if folder in ('desk_clock','home_fonts'):shutil.copyfile(ROOT/'lib/PortableApps'/folder/'SOURCES.json',target/'SOURCES.json')
 if a.sparse_start:shutil.copytree(ROOT/'lib/PortableApps/boot_logo',out/'licenses/boot_logo',dirs_exist_ok=True)
 print('Paper default clock: target structural validation and imports/exports passed')
if __name__=='__main__':build()

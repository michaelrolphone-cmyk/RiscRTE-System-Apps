"""Explicit X4 reversible-idle profile; no native grants by default."""
from pathlib import Path
import hashlib
import shutil
VERSIONS={'paper_clock':'0.3.12','springboard':'1.7.9','settings':'1.3.17','file_browser':'1.5.12','wifi_settings':'1.1.14','ota_update':'1.2.3','app_store':'1.2.3'}
HEADERS=('RiscDisplayOutputV1.h','RiscDisplayOutputPowerV1.h','RiscTouchV1.h','RiscTouchPowerV1.h','RiscStorageVolumeV1.h')
FLAGS=['-DPORTABLE_X4_IDLE_POLICY','-DPORTABLE_LOW_BATTERY','-DPORTABLE_APP_SLEEP_LOCAL']

def options(p):
 p.add_argument('--x4-idle-source',type=Path,help='Explicit X4 typed reversible Light helper; enables automatic idle and low-battery policy')
 p.add_argument('--x4-idle-sdk',type=Path,help='Canonical typed panel/touch/storage driver SDK')
 p.add_argument('--x4-idle-runtime-sdk',type=Path,help='Canonical Runtime driver SDK containing timed/Light sleep headers')

def selected(a):return bool(getattr(a,'x4_idle_source',None))
def version(a,app,current):
 if getattr(a,"touch_scrolling",False) and app in ("ota_update","app_store"):return "1.2.4"
 return VERSIONS[app] if selected(a) else current

def configure(a,p,root,out,base):
 values=[getattr(a,n,None) for n in ('x4_idle_source','x4_idle_sdk','x4_idle_runtime_sdk')]
 if not any(values):return [],[]
 if not all(values):p.error('X4 idle requires --x4-idle-source, --x4-idle-sdk and --x4-idle-runtime-sdk')
 if not getattr(a,'paper_transitions',False):p.error('X4 idle requires --paper-transitions for confirmed OFF-capable brightness controls')
 if not a.quick_actions or not a.quick_radios or not a.alarm_client or not getattr(a,'tagged_alarm_utilities',None):
  p.error('X4 idle requires Quick Controls, radios and tagged alarm client')
 if not (getattr(a,'sparse_start',False) or getattr(a,'settings_profile',None)=='x4-native-time' or getattr(a,'time_profile',None)=='x4-native-time'):
  p.error('X4 idle requires sparse Clock or the native time app profile')
 if not base:p.error('X4 idle requires the canonical staged SDK')
 directory=out/'idle-sdk';includes=directory/'include'
 shutil.copytree(base,includes,dirs_exist_ok=True)
 shutil.copytree(root/'lib/PortableApps/time',directory/'time',dirs_exist_ok=True)
 source={name:(a.x4_idle_sdk/name).read_bytes() for name in HEADERS}
 source.update({name:(a.x4_idle_runtime_sdk/name).read_bytes() for name in ('RiscLightSleepV1.h','RiscTimedSleepV1.h','RiscDeepSleepV1.h')})
 for name,data in source.items():(includes/name).write_bytes(data)
 mode='Home idle: desk-lock Deep; resident foreground: reversible Light with app state retained' if getattr(a,'desk_lock_home',False) else 'Light only; foreground state retained'
 a.x4_idle_receipt={'enabled':True,'mode':mode,'low_battery_threshold_percent':10,
  'idle_default_ms':60000,'low_battery_idle_ms':20000,'foreground_capture_restart':False,'background_radios':'restore confirmed saved intent after typed resume',
  'source':str(a.x4_idle_source.resolve()),'helper_sha256':hashlib.sha256(a.x4_idle_source.read_bytes()).hexdigest(),
  'build_source_sha256':{n:hashlib.sha256((root/n).read_bytes()).hexdigest() for n in ('scripts/portable_idle_build.py','scripts/portable_quick_build.py')},
  'compiled_include_directory':str(includes.resolve()),'sdk_sha256':{n:hashlib.sha256(d).hexdigest() for n,d in source.items()},
  'system_source_sha256':{str(f.relative_to(root)):hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted((root/'lib/PortableApps').rglob('*')) if f.is_file()},
  'hardware_qualification':False,'physical_instances':{'x4.power':17,'display.output':3,'input.touch.raw':4,'storage.volume':9,'net.wifi':15,'bluetooth.hci':16},'preferences_instance':1,'alarm_service':{'api':2,'instance':0},'unused_timer_record':'sleep_deep'}
 return [*FLAGS,'-I'+str(includes)],[a.x4_idle_source]

def requirements(a,needs):
 if not selected(a):return
 for name in ('x4.power','storage.volume'):
  item={'capability':name,'api':1}
  if item not in needs:needs.append(item)

def record(a,record,flags):
 if not selected(a):return
 record['idle_policy']=a.x4_idle_receipt
 record['build_defines']=flags
 if 'mode_capabilities' in record:record['mode_capabilities'].update(automatic_idle_light=not getattr(a,'desk_lock_home',False),sleep_backend=True)
 record.setdefault('preferences',{}).update(idle_timer_key='sleep_idle',unused_deep_timer_key='sleep_deep')
 if 'required_grants' in record:
  for grant in record['required_grants']:
   mapping=a.x4_idle_receipt['physical_instances']
   if grant['capability'] in mapping:grant['instance_id']=mapping[grant['capability']]
  record.setdefault('grant_bindings',{}).update({k:[v] for k,v in a.x4_idle_receipt['physical_instances'].items()})

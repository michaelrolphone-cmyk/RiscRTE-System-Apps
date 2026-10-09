#!/usr/bin/env python3
"""Real Clock/controller/adapter/public X4 client composition, not real providers.

Runs normal + ASan/UBSan over strict native/provider doubles, exact prior-image
fresh-process reconstruction and independent ZoneInfo render comparisons.
The pinned target is a development ELF only. No product/BIN/catalog changes.
"""
import argparse, hashlib, io, json, os, shutil, subprocess, tarfile, tempfile
from datetime import datetime, timezone
from pathlib import Path
from zoneinfo import ZoneInfo
ROOT=Path(__file__).resolve().parents[1]
BASE='d52a74bfb4acc01cc3f0a9dda2c95ba2ba679ee9'
X4_REF='1cc18a9c998a3d041cf25364fb15c98449fe99d3'
RUNTIME_REF='602ae9bd618e13407b5b94bcad86cdabc23c99ea'
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--runtime-sdk',type=Path,required=True)
p.add_argument('--runtime-ref',default=RUNTIME_REF,help='Exact canonical Runtime Git object providing tested SDK')
p.add_argument('--sdk',type=Path,required=True)
p.add_argument('--x4',type=Path,required=True)
p.add_argument('--xtensa-cc',required=True)
p.add_argument('--evidence',type=Path)
a=p.parse_args()
FLAGS=['-DTEST_NATIVE_LANDSCAPE','-DPORTABLE_DISPLAY_ROTATION=90','-DPORTABLE_APP_OWNS_TOUCH_CHROME',
 '-DPORTABLE_RTC_WALL_TIME','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_APP_SLEEP_LOCAL',
 '-DPORTABLE_CROWN_SLEEP_LOCAL','-DPORTABLE_SLEEP_MANUAL_ONLY','-DPORTABLE_DESK_CLOCK','-DPORTABLE_DESK_CLOCK_SPARSE_START']
NAMES=['adapter.c','desk_clock_faces.c','PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c']
SOURCES=[ROOT/'Apps/paper_clock.c',*[ROOT/'lib/PortableApps/src'/n for n in NAMES],a.x4/'minimal/apps/portable_sleep.c']
QUICK=[ROOT/'lib/PortableApps/src'/n for n in ('quick_actions.c','quick_render.c','quick_session.c','quick_radios.c')]
CASES='terminal cold gpio invalid-record unset absent-native invalid-native refused held cancel promotion-failed promotion-partial promotion-retained promotion-ready record-context native-context native-release native-acquire-retained rtc-acquire-retained rtc-read-retained panel-retained panel-refused retained resume-retained clear-retained release-retained key-retained stage-context stage-refused alarm-due slow-prepare cross-minute missing-zone missing-basis bad-zone bad-basis manual alarm-output alarm-uncertain refused-promotion-failed refused-promotion-partial init-nosuffix init-missing-barrier'.split()
HEADERS=['RiscRuntimeV1.h','RiscRealtimeV1.h','RiscProviderPromotionV1.h','RiscRetainedWakeV1.h']
DRIVERS=['RiscDisplayOutputV1.h','RiscDisplayOutputPowerV1.h','RiscTouchV1.h','RiscTouchPowerV1.h','RiscStorageVolumeV1.h']
def run(args,**kw):return subprocess.run(list(map(str,args)),check=True,**kw)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
assert subprocess.check_output(['git','-C',a.x4,'show',X4_REF+':minimal/apps/portable_sleep.c'])==(a.x4/'minimal/apps/portable_sleep.c').read_bytes()
runtime_repo=a.runtime_sdk.resolve().parents[1]
for n in HEADERS:
 assert subprocess.check_output(['git','-C',runtime_repo,'show',a.runtime_ref+':sdk/app/'+n])==(a.runtime_sdk/n).read_bytes(),n
runtime_tree=subprocess.check_output(['git','-C',runtime_repo,'rev-parse',a.runtime_ref+'^{tree}'],text=True).strip()
evidence={'runtime_ref':a.runtime_ref,'runtime_tree':runtime_tree,'purpose':'host-controller-adapter-client-composition-and-target-ELF-not-hardware','x4_client_ref':X4_REF,
 'clock_policy':'native-realtime-iana','sparse_start':True,'provider_activation':'demand',
 'timer_preferences':'retained-only','foreground_promotion':True,'invocation_retention':True,'grant_count':14,
 'sdk_headers':{n:sha(a.runtime_sdk/n) for n in HEADERS},'driver_headers':{n:sha(a.sdk/n) for n in DRIVERS},
 'tested_app_live_grant_high_water':{'timer':6,'foreground_deep':12},
 'runtime_custody_qualification':'separate real-Runtime tests required; local fences alone are not proof',
 'physical_qualification':'not run'}
count=0
with tempfile.TemporaryDirectory(prefix='sparse-clock-startup-') as tmp:
 out=Path(tmp);include=out/'include';shutil.copytree(ROOT/'lib/PortableApps/include',include)
 shutil.copytree(ROOT/'lib/PortableApps/time',out/'time')
 for n in DRIVERS:shutil.copyfile(a.sdk/n,include/n)
 for n in HEADERS:shutil.copyfile(a.runtime_sdk/n,include/n)
 includes=['-I'+str(include),'-I'+str(ROOT/'lib/NativeApps/include'),'-I'+str(a.runtime_sdk.parent/'driver'),'-I'+str(a.x4/'minimal/drivers/x4pro_power')]
 for san in (False,True):
  for quick in (False,True):
   binary=out/'fixture';flags=[*FLAGS]
   if quick:flags+=['-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_QUICK_RADIOS']
   if san:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
   run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*includes,
        *SOURCES,ROOT/'test/native_apps/sparse_clock_startup_test.c',*(QUICK if quick else []),'-o',binary])
   env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0')
   def case(name,path=None,**extra):
    global count
    count+=1
    if path is None:
     path=out/'state';path.unlink(missing_ok=True)
    run([binary,name,path],env=dict(env,**extra),timeout=20,stdout=subprocess.DEVNULL)
    return path
   for name in CASES:case(name)
   case('terminal',CLOCK_MILLIS='4294967195')
   for name in ('fold','gap'):case(name,CLOCK_ZONE='America/Denver')
   # Corrupt policy and unchosen DST folds/gaps remain unavailable. Missing
   # records use the documented Reader defaults and recover the RTC instead.
   images=[]
   for name in ('bad-zone','bad-basis','fold','gap'):
    capture=out/(name+'.pixels');case(name,PAPER_FRAME=str(capture),CLOCK_ZONE='America/Denver');images.append(capture.read_bytes())
   assert all(x==images[0] for x in images)
   for name in ('missing-zone','missing-basis'):
    capture=out/(name+'.pixels');case(name,PAPER_FRAME=str(capture),CLOCK_ZONE='America/Denver');assert capture.read_bytes()!=images[0]
   valid=out/'valid.pixels';case('foreground',PAPER_FRAME=str(valid),CLOCK_ZONE='America/Denver');assert valid.read_bytes()!=images[0]
   # Exact old-image pixels, all six faces, both orientations, refresh rollover,
   # retained language and poisoned live preference inputs on subsequent boots.
   for face in range(6):
    for flip in (0,1):
     state=out/f'{san}-{quick}-{face}-{flip}.state'
     for cycle in range(32):
      case('terminal',state,CLOCK_FACE=str(face),CLOCK_FLIP=str(flip),CLOCK_LANG='21',
           CLOCK_ZONE='America/Denver' if cycle==0 else 'Poison/Unused')
     case('seed-retained',state)
   # Render equality against Python's independent zoneinfo UTC->local oracle,
   # including both sides of spring-forward and fall-back, fractional offsets,
   # date/year boundaries, and the same display orientation.
   for zone,dates in [('America/Denver',['2026-03-08T08:59:57','2026-03-08T09:00:00','2026-11-01T07:59:57','2026-11-01T08:00:00']),
                      ('Australia/Lord_Howe',['2026-04-04T14:59:57','2026-04-04T15:00:00']),
                      ('Asia/Kathmandu',['2026-12-31T23:59:57']),('Pacific/Chatham',['2026-09-26T14:00:00'])]:
    for date in dates:
     stamp=int(datetime.fromisoformat(date).replace(tzinfo=timezone.utc).timestamp())
     local=int(datetime.fromtimestamp(stamp,ZoneInfo(zone)).replace(tzinfo=timezone.utc).timestamp())
     for face in range(6):
      first=case('terminal',CLOCK_ZONE=zone,CLOCK_EPOCH=str(stamp),CLOCK_FACE=str(face)).read_bytes()[-48000:]
      second=case('terminal',CLOCK_ZONE='UTC',CLOCK_EPOCH=str(local),CLOCK_FACE=str(face)).read_bytes()[-48000:]
      assert first==second,(zone,date,face)
   print(f'Real sparse Clock: {"ASan/UBSan" if san else "normal"}, quick={quick} composition passed',flush=True)
 # Build the actual selected production ELF through the production builder.
 targets=[]
 for quick in (False,True):
  target=out/('target-quick' if quick else 'target')
  run([os.environ.get('PYTHON','python3'),ROOT/'scripts/build_paper_clock.py','--desk-clock','--sparse-start','--navigation','--alarm-client',
       '--sleep-capability','x4.power','--sleep-sdk',a.sdk,'--retained-wake-sdk',a.runtime_sdk,
       '--local-sleep-source',a.x4/'minimal/apps/portable_sleep.c','--output-dir',target,
       *(['--quick-actions','--quick-radios'] if quick else [])],env=dict(os.environ,NATIVE_APP_CC=a.xtensa_cc))
  receipt=json.loads((target/'build-evidence.json').read_text());assert receipt['grant_count']==14
  assert receipt['version']=='0.3.2' and receipt['sparse_start'] and receipt['invocation_retention']
  targets.append(receipt)
 # Actual unchanged default artifacts against the exact public base. A trusted
 # local Git archive preserves original relative includes without a worktree.
 base=out/'base';base.mkdir()
 archive=subprocess.check_output(['git','-C',ROOT,'archive',BASE])
 with tarfile.open(fileobj=io.BytesIO(archive)) as tar:tar.extractall(base,filter='data')
 # The legacy builders require Git metadata for their auxiliary receipts.
 # Build from the verified archived tree with a local synthetic metadata commit;
 # only ELF bytes are consumed below, never that synthetic source identity.
 run(['git','init','-q',base])
 run(['git','-C',base,'-c','user.name=Fixture','-c','user.email=fixture@invalid','commit','-q','--allow-empty','-m','Archived public base witness'])
 compatible={}
 for name,script,args,filename in [
     ('paper','build_paper_clock.py',['--navigation','--alarm-client','--quick-actions','--quick-radios'],'default.elf'),
     ('watch','build_portable_springboard.py',['--nova-ui','--navigation','--alarm-client','--denver','--quick-actions','--quick-radios'],'springboard.elf')]:
  hashes=[]
  for index,repo in enumerate((base,ROOT)):
   destination=out/f'{name}-{index}'
   run([os.environ.get('PYTHON','python3'),repo/'scripts'/script,*args,'--output-dir',destination],env=dict(os.environ,NATIVE_APP_CC=a.xtensa_cc))
   hashes.append(sha(destination/filename))
  assert hashes[0]==hashes[1],name+' default ELF bytes changed'
  compatible[name]=hashes[0]
 evidence['default_elf_compatibility']={'base':BASE,'sha256':compatible}
 evidence['targets']=targets
 evidence['process_cases']=count
 tracked=[*SOURCES,*QUICK,ROOT/'Apps/paper_sparse_clock.inc',ROOT/'lib/PortableApps/src/sparse_clock_adapter.inc',ROOT/'lib/PortableApps/src/alarm.inc',ROOT/'lib/PortableApps/src/nova.inc',ROOT/'lib/PortableApps/src/quick_adapter.inc',
          ROOT/'test/native_apps/sparse_clock_startup_test.c',ROOT/'scripts/build_paper_clock.py',Path(__file__).resolve()]
 evidence['source_sha256']={str(f.relative_to(ROOT)) if f.is_relative_to(ROOT) else 'x4/minimal/apps/portable_sleep.c':sha(f) for f in tracked}
 if a.evidence:a.evidence.parent.mkdir(parents=True,exist_ok=True);a.evidence.write_text(json.dumps(evidence,indent=2)+'\n')
 print(f'{count} normal/sanitized fresh-process composition cases; two production target ELFs PASS')

#!/usr/bin/env python3
"""Actual sparse Clock/adapter/product sleep: copied catalog expiry and rewind."""
import argparse, hashlib, json, os, shutil, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--candidate',type=Path,required=True,help='Qualified host target providing exact compiled SDK headers')
p.add_argument('--sleep-source',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--baseline',type=Path,help='Optional clean pre-fix worktree for defect reproduction')
a=p.parse_args();out=a.output.resolve();include=out/'include';include.mkdir(parents=True,exist_ok=True)
receipt=json.loads((a.candidate/'build-evidence.json').read_text())
shutil.copytree(receipt['paper_transition']['compiled_include_directory'],include,dirs_exist_ok=True)
for name in ('PortableHomePointsCatalog.h','PortableDeskPointsSnapshot.h','PortablePointsCatalogView.h','PortableDeskClock.h','PortablePointsState.h'):
 shutil.copyfile(ROOT/'lib/PortableApps/include'/name,include/name)
shutil.copytree(ROOT/'lib/PortableApps/time',out/'time',dirs_exist_ok=True)
flags=['-DTEST_NATIVE_LANDSCAPE','-DPORTABLE_DISPLAY_ROTATION=90','-DPORTABLE_APP_OWNS_TOUCH_CHROME',
 '-DPORTABLE_RTC_WALL_TIME','-DPORTABLE_ALARM_CLIENT','-DALARM_SERVICE_TAGGED_V2','-DPORTABLE_ALARM_TERMINAL_RETENTION',
 '-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_APP_SLEEP_LOCAL','-DPORTABLE_CROWN_SLEEP_LOCAL','-DPORTABLE_SLEEP_MANUAL_ONLY',
 '-DPORTABLE_DESK_CLOCK','-DPORTABLE_DESK_CLOCK_SPARSE_START','-DPORTABLE_DESK_LOCK_HOME','-DPORTABLE_DESK_POINTS_FACE',
 '-DPORTABLE_DESK_POINTS_SNAPSHOT','-DPORTABLE_HOME_POINTS_NATIVE_UTC','-DALARM_NATIVE_UTC']
names='adapter.c desk_clock_faces.c PortableRealtimeClient.c PortableTimeZone.c PortableTimeZoneCatalog.c PortableTimeZonePreference.c'.split()
sources=[ROOT/'Apps/paper_clock.c',*[ROOT/'lib/PortableApps/src'/n for n in names],a.sleep_source,ROOT/'test/native_apps/sparse_points_expiry_test.c']
quick=[ROOT/'lib/PortableApps/src'/n for n in 'quick_actions.c quick_render.c quick_session.c quick_radios.c'.split()]
incs=['-I'+str(include),'-I'+str(ROOT/'lib/NativeApps/include'),'-I'+str(a.sleep_source.parent.parent/'drivers/x4pro_power')]
cases='valid expired rewind rewind-in-window unavailable other-face cross-minute budget due rtc-error blocked owned projection-error stale-result future-result old-snapshot status-terminal uncertain pump-status-terminal step-uncertain refresh-terminal step-terminal projection-terminal'.split()
runs=[]
for sanitized in (False,True):
 label='asan-ubsan' if sanitized else 'normal';directory=out/label;directory.mkdir(exist_ok=True)
 extra=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
 for qa in (False,True):
  binary=directory/('fixture-'+str(int(qa)))
  qflags=['-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_QUICK_RADIOS','-DPORTABLE_DESK_WAKE_LIGHT','-DPORTABLE_PAPER_TRANSITIONS'] if qa else []
  command=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*extra,*flags,*qflags,*incs,*map(str,sources),*map(str,quick if qa else []),'-o',str(binary)]
  subprocess.run(command,check=True)
  for case in cases:
   for flip in ('0','1'):
    for asynchronous in (False,True):
     image=directory/f'{int(qa)}-{case}-{flip}-{int(asynchronous)}.pbm'
     env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1','EXPIRY_FLIP':flip}
     if asynchronous:env['EXPIRY_ASYNC']='1'
     result=subprocess.run([binary,case,image],env=env,capture_output=True,text=True,timeout=20)
     (directory/'latest.log').write_text(result.stdout+result.stderr)
     assert result.returncode==0,(label,qa,case,flip,asynchronous,result.stdout,result.stderr)
     runs.append({'profile':label,'quick_wake_light':qa,'case':case,'flip':flip,'async':asynchronous,'result':result.stdout.strip()})
  # Commit the real 408-byte refreshed payload, reboot with it unchanged, then
  # advance to its exclusive expiry. Both fresh processes use the same app.
  for flip in ('0','1'):
   prior=directory/f'{int(qa)}-expired-{flip}-0.pbm.record'
   for case in ('valid','expired'):
    image=directory/f'{int(qa)}-reboot-{case}-{flip}.pbm'
    result=subprocess.run([binary,case,image],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1','EXPIRY_RECORD':str(prior)},capture_output=True,text=True,timeout=20)
    assert result.returncode==0,(label,qa,case,flip,result.stdout,result.stderr)
    runs.append({'profile':label,'quick_wake_light':qa,'case':'reboot-'+case,'flip':flip,'result':result.stdout.strip()})
    prior=Path(str(image)+'.record')
  print(f'{label} Quick/radios/wake-light={qa}: dark TIMER catalog expiry/rewind and terminal fences PASS',flush=True)
for path in (out/'normal').glob('*.pbm'):assert path.read_bytes()==(out/'asan-ubsan'/path.name).read_bytes(),path
baseline=[]
if a.baseline:
 old_sources=[a.baseline/path.relative_to(ROOT) if path.is_relative_to(ROOT) and path.name!='sparse_points_expiry_test.c' else path for path in sources]
 binary=out/'baseline'
 subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-DTEST_POINTS_EXPIRY_BASELINE',*incs,*map(str,old_sources),'-o',str(binary)],check=True)
 for case in ('expired','rewind','rewind-in-window','valid'):
  result=subprocess.run([binary,case,out/'baseline.pbm'],capture_output=True,text=True,timeout=20)
  assert (result.returncode==0)==(case=='valid'),(case,result.stdout,result.stderr)
  baseline.append({'case':case,'returncode':result.returncode,'result':result.stdout+result.stderr})
 (out/'baseline-reproduction.json').write_text(json.dumps(baseline,indent=2)+'\n')
tracked=[*sources,*quick,ROOT/'Apps/paper_sparse_clock.inc',ROOT/'Apps/paper_home_points.inc',ROOT/'lib/PortableApps/src/sparse_clock_adapter.inc',ROOT/'lib/PortableApps/include/PortableHomePointsCatalog.h',ROOT/'lib/PortableApps/include/PortablePointsState.h',ROOT/'test/native_apps/sparse_clock_startup_test.c',Path(__file__)]
(out/'receipt.json').write_text(json.dumps({'source':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True)),
 'runs':runs,'process_cases':len(runs),'pixels':'normal and ASan/UBSan byte identical','snapshot_abi_bytes':408,
 'candidate_sdk':str(a.candidate),'baseline':baseline,'source_sha256':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in tracked},'hardware':'not run'},indent=2)+'\n')
print(f'{len(runs)} actual TIMER controller/adapter/sleep cases PASS')

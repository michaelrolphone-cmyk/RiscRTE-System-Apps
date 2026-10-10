#!/usr/bin/env python3
"""Qualify actual Home catalog raster and schema3 codec without provider I/O."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--utilities',type=Path,required=True)
p.add_argument('--utilities-revision',default='HEAD',help='Exact selected service source revision (resolved and recorded)')
p.add_argument('--runtime-sdk',type=Path,required=True)
p.add_argument('--output-dir',type=Path,required=True)
a=p.parse_args();out=a.output_dir.resolve();include=out/'include';include.mkdir(parents=True,exist_ok=True)
PIN=subprocess.check_output(['git','-C',a.utilities,'rev-parse',a.utilities_revision],text=True).strip()
for path in (ROOT/'lib/PortableApps/include').glob('*.h'):shutil.copyfile(path,include/path.name)
shutil.copytree(ROOT/'lib/PortableApps/time',out/'time',dirs_exist_ok=True)
for name in ('RiscRuntimeV1.h','RiscRealtimeV1.h','RiscProviderPromotionV1.h','RiscRetainedWakeV1.h'):
 shutil.copyfile(a.runtime_sdk/name,include/name)
for name in ('AlarmRecords.h','PointsRecords.h','PointsSchedule.h','PointsUtcSchedule.h','PointsCatalogProjection.h'):
 (include/name).write_bytes(subprocess.check_output(['git','-C',a.utilities,'show',PIN+':lib/Alarm/include/'+name]))
flags=['-DPORTABLE_DESK_CLOCK','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_APP_SLEEP_LOCAL',
 '-DPORTABLE_DESK_CLOCK_SPARSE_START','-DPORTABLE_DESK_LOCK_HOME','-DPORTABLE_DESK_POINTS_FACE',
 '-DPORTABLE_DESK_POINTS_SNAPSHOT','-DPORTABLE_HOME_POINTS_NATIVE_UTC','-DALARM_NATIVE_UTC']
sources=[ROOT/'test/native_apps/home_catalog_render_test.c',ROOT/'lib/PortableApps/src/PortableTimeZone.c',ROOT/'lib/PortableApps/src/PortableTimeZoneCatalog.c']
runs=[]
for sanitized in (False,True):
 label='asan-ubsan' if sanitized else 'normal';directory=out/label;directory.mkdir(exist_ok=True)
 extra=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
 common=['cc','-std=c11','-O1','-Wall','-Wextra','-Werror',*extra,*flags,'-I'+str(include),'-I'+str(ROOT/'lib/NativeApps/include')]
 env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
 for name,inputs in [('render',sources),('codec',[ROOT/'test/native_apps/desk_points_catalog_snapshot_test.c'])]:
  binary=directory/name
  subprocess.run([*common,'-ffunction-sections','-fdata-sections','-Wl,--gc-sections',*map(str,inputs),'-o',binary],check=True)
  result=subprocess.run([binary,*([directory] if name=='render' else [])],check=True,env=env,capture_output=True,text=True)
  (directory/(name+'.log')).write_text(result.stdout+result.stderr);runs.append({'profile':label,'test':name,'result':result.stdout.strip()})
 for path in directory.glob('*.pbm'):Image.open(path).save(path.with_suffix('.png'))
for path in (out/'normal').glob('*.pbm'):assert path.read_bytes()==(out/'asan-ubsan'/path.name).read_bytes()
inputs=[*sources,ROOT/'Apps/paper_clock.c',ROOT/'Apps/paper_home_points.inc',ROOT/'Apps/paper_home_type.inc',ROOT/'Apps/PaperHomeCatalogPoints.h',ROOT/'Apps/paper_sparse_clock.inc',ROOT/'lib/PortableApps/include/PortablePointsCatalogView.h',ROOT/'lib/PortableApps/include/PortableDeskPointsSnapshot.h',ROOT/'lib/PortableApps/include/PortableDeskClock.h',ROOT/'lib/PortableApps/include/PortablePointsState.h']
(out/'evidence.json').write_text(json.dumps({'purpose':__doc__,'utilities_commit':PIN,'runs':runs,
 'source_sha256':{str(path.relative_to(ROOT)):hashlib.sha256(path.read_bytes()).hexdigest() for path in inputs},
 'sdk_sha256':{path.name:hashlib.sha256(path.read_bytes()).hexdigest() for path in include.glob('*.h')},
 'hardware':'not run','limits':'Direct actual-render and pure-codec tests; native retained transport and app custody are qualified separately.'},indent=2)+'\n')
print('Home catalog: actual renderer/codec passed normal and ASan/UBSan; matching pixels, full labels, schema separation and expiry')

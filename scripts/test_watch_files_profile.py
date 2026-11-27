#!/usr/bin/env python3
"""Real Files app/adapter raster and native reader; modeled providers/Light."""
import argparse,os,shutil,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
r=Path(__file__).resolve().parents[1];out=a.output.resolve();inc=out/'sdk/include'
shutil.copytree(r/'lib/PortableApps/include',inc,dirs_exist_ok=True)
shutil.copytree(r/'lib/PortableApps/time',inc.parent/'time',dirs_exist_ok=True)
for f in (a.runtime/'sdk/app').glob('*.h'):shutil.copyfile(f,inc/f.name)
common=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function']
if a.sanitize:common+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
flags=['PORTABLE_WATCH_NATIVE_HEIGHT','PORTABLE_WATCH_COLOR_ONLY','PORTABLE_WATCH_SAFE_INSETS','PORTABLE_SAFE_VIEWPORT','PORTABLE_FILE_BROWSER_READ_COPY_ONLY','PORTABLE_FILE_BROWSER_RGB_ONLY','PORTABLE_NATIVE_TIME_TOOLBAR','PORTABLE_NATIVE_CUSTODY_FENCE','PORTABLE_FOREGROUND_IDLE','PORTABLE_APP_SLEEP_LOCAL','PORTABLE_INPUT_NAVIGATION']
common += ['-DPORTABLE_HOME_APP="default.elf"','-DFILE_BROWSER_RETURN_APP="default.elf"']+['-D'+f for f in flags]+['-I'+str(inc),'-I'+str(r/'lib/NativeApps/include')]
sources=[r/'test/native_apps/watch_files_profile_test.c']+[r/'lib/PortableApps/src'/n for n in ['PortableNativeTimeSource.c','PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c']]
for w,h,insets,round_panel in [(240,240,(0,0,0,0),False),(466,466,(69,69,69,69),True),(410,502,(5,11,13,17),False),(480,600,(12,30,24,15),False),(300,320,(20,10,20,15),False)]:
 exe=out/f'{w}x{h}';defs=[f'-DFILE_TEST_WIDTH={w}',f'-DFILE_TEST_HEIGHT={h}']
 defs += ['-DTEST_INSET_'+name+'='+str(n) for name,n in zip(['LEFT','TOP','RIGHT','BOTTOM'],insets)]
 if round_panel:defs+=['-DTEST_ROUND']
 subprocess.run([*common,*defs,*map(str,sources),'-o',str(exe)],check=True)
 for case in ['ordinary','retained']:
  frames=out/f'{w}x{h}-{case}';frames.mkdir(exist_ok=True)
  subprocess.run([exe,case,frames],check=True,timeout=45,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
print('Watch Files generalized geometry/clock/ownership/Light matrix PASS; physical hardware untested')

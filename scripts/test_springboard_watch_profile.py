#!/usr/bin/env python3
"""Exercise explicit Watch presentation with real app/client and panel metadata."""
import argparse, os, subprocess, shutil
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');p.add_argument('--runtime',type=Path,required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[1];a.output.mkdir(parents=True,exist_ok=True)
base=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror']
if a.sanitize:base+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-sanitize-recover=all','-no-pie']
base+=['-I'+str(r/'lib/PortableApps/include'),'-I'+str(r/'lib/NativeApps/include')]
sources=[str(r/x) for x in ['Apps/springboard.c','lib/PortableApps/src/adapter.c','test/native_apps/springboard_nova_test.c']]
for width,height in [(240,240),(466,466),(410,502),(240,320)]:
 for mode,extra,expected in [('unknown',['-DTEST_UNKNOWN_REFRESH'],1),('known',[],1),('slow',['-DTEST_SLOW_REFRESH'],0),('retaining',['-DTEST_RETAINING_PANEL'],0)]:
  exe=a.output/f'{width}x{height}-{mode}'
  flags=['-DPORTABLE_NOVA_UI','-DPORTABLE_WATCH_NATIVE_HEIGHT','-DPORTABLE_WATCH_COLOR_ONLY',f'-DTEST_WIDTH={width}',f'-DTEST_HEIGHT={height}',f'-DTEST_WATCH_PRESENTATION={expected}',*extra]
  subprocess.run([*base,*flags,*sources,'-Wl,--wrap=t5_app_get_api','-o',str(exe)],check=True)
  # Capability-only rejection checks do not drive a fallback UI with Nova gestures.
  for case in ([0,1,2,6,14,15,16,23,24,36,37] if expected else [39]):
   subprocess.run([str(exe),str(case)],check=True,timeout=30)
print('Watch native presentation: metadata admission and interrupted/repeated input paths passed')

# Same production native reader and selected-timezone owner as the target ELF.
include=a.output/'native-sdk/include'
shutil.copytree(r/'lib/PortableApps/include',include,dirs_exist_ok=True)
shutil.copytree(r/'lib/PortableApps/time',include.parent/'time',dirs_exist_ok=True)
for name in ['RiscRuntimeV1.h','RiscRealtimeV1.h']:
 shutil.copyfile(a.runtime/'sdk/app'/name,include/name)
helpers=[str(r/'lib/PortableApps/src'/name) for name in ['PortableNativeTimeSource.c','PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c']]
for width,height in [(240,240),(466,466),(410,502),(240,320)]:
 exe=a.output/f'{width}x{height}-native-time'
 flags=['-DPORTABLE_NOVA_UI','-DPORTABLE_WATCH_NATIVE_HEIGHT','-DPORTABLE_WATCH_COLOR_ONLY','-DTEST_UNKNOWN_REFRESH','-DPORTABLE_NATIVE_TIME_TOOLBAR','-DPORTABLE_NATIVE_CUSTODY_FENCE','-DTEST_NATIVE_TIME',f'-DTEST_WIDTH={width}',f'-DTEST_HEIGHT={height}']
 subprocess.run([*base[:base.index('-I'+str(r/'lib/PortableApps/include'))],'-I'+str(include),'-I'+str(r/'lib/NativeApps/include'),*flags,*sources,*helpers,'-Wl,--wrap=t5_app_get_api','-o',str(exe)],check=True)
 for case in [0,1,2,6,14,15,16,23,24,36,37,40,41,42]:
  subprocess.run([str(exe),str(case)],check=True,timeout=30)
print('Watch native readonly clock: actual native owner, UTC/missing, Denver, UNSET and IO cases passed')

#!/usr/bin/env python3
"""Real Springboard controller/adapter lifecycle with deterministic providers."""
import argparse, os, shutil, subprocess, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--runtime',type=Path,required=True)
p.add_argument('--sdk',type=Path,required=True)
p.add_argument('--alarm-sdk',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
inc=out/'include';shutil.copytree(ROOT/'lib/PortableApps/include',inc,dirs_exist_ok=True)
shutil.copytree(ROOT/'lib/PortableApps/time',out/'time',dirs_exist_ok=True)
for name in ('RiscRuntimeV1.h','RiscRealtimeV1.h','RiscResidentShellV1.h'):
 shutil.copyfile(a.runtime/'sdk/app'/name,inc/name)
for name in ('RiscStorageVolumeV1.h','RiscDisplayOutputV1.h','RiscDisplayOutputPowerV1.h','RiscDisplayOutputMetricsV1.h','RiscDisplayOutputSnapshotV1.h'):
 src=a.sdk/name
 if src.exists():shutil.copyfile(src,inc/name)
for name in ('AlarmServiceV1.h','AlarmServiceV2.h'):shutil.copyfile(a.alarm_sdk/name,inc/name)
flags='PORTABLE_ALARM_CLIENT PORTABLE_ALARM_TERMINAL_RETENTION ALARM_SERVICE_TAGGED_V2 PORTABLE_TOUCH_SCROLL PORTABLE_PAPER_TRANSITIONS PORTABLE_NATIVE_CUSTODY_FENCE PORTABLE_NATIVE_TIME_TOOLBAR PORTABLE_INPUT_NAVIGATION PORTABLE_APP_TOUCH_SCROLL PORTABLE_NOVA_UI PORTABLE_SPRINGBOARD_TOUCH_SCROLL PORTABLE_RESIDENT_SHELL_CLIENT PORTABLE_RESIDENT_POLICY PORTABLE_INSTALLED_CATALOG PORTABLE_RASTER_SNAPSHOT'.split()
flags+=['PORTABLE_HOME_APP="default.elf"','PORTABLE_INSTALLED_VOLUME_INSTANCE=9u','PORTABLE_SPRINGBOARD_CATALOG_BOUND=40']
sources=[ROOT/'Apps/springboard.c',ROOT/'test/native_apps/installed_springboard_lifecycle_test.c']
sources += [ROOT/'lib/PortableApps/src'/f'{n}.c' for n in ('PortableInstalledCatalog','PortableNativeTimeSource','PortableRealtimeClient','PortableTimeZone','PortableTimeZoneCatalog','PortableTimeZonePreference')]
link=['-Wl,--wrap=free']
if sys.platform=='darwin':
 # This suite tests successful cleanup, not the separate allocator-retention
 # oracle. GNU wrapping remains enabled on supported hosts.
 shim=out/'free.c';shim.write_text('#include <stdlib.h>\nvoid __real_free(void*p){free(p);}\n');sources.append(shim);link=[]
exe=out/'test'
subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-fsanitize=undefined','-fno-sanitize-recover=all',*['-D'+x for x in flags],'-I'+str(inc),'-I'+str(ROOT/'lib/NativeApps/include'),'-I'+str(a.runtime/'sdk/driver'),*map(str,sources),*link,'-o',str(exe)],check=True)
for case in ('launch','home','controls','refused'):
 for latency in (0,17,2300):subprocess.run([str(exe),case,str(latency)],check=True,timeout=30)

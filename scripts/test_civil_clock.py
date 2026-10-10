#!/usr/bin/env python3
import argparse,os,shutil,subprocess,tempfile
from pathlib import Path
from scene_sdk import stage_sdk
ROOT=Path(__file__).resolve().parents[1]
def run(runtime):
 with tempfile.TemporaryDirectory(prefix='civil-test-') as tmp:
  out=Path(tmp);inc=stage_sdk(runtime,ROOT,out)
  for name in ('PortableTimeZone.h','PortableTimeZonePreference.h','PortableRtcClock.h','PortableTime.h'):shutil.copyfile(ROOT/'lib/PortableApps/include'/name,inc/name)
  shutil.copytree(ROOT/'lib/PortableApps/time',out/'time')
  for native in (False,True):
   sources=[ROOT/'Services/civil_clock/clock.c',ROOT/'test/civil_clock/clock_test.c',*[ROOT/'lib/PortableApps/src'/n for n in ('PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c')]]
   exe=out/('utc' if native else 'raw');flags=['-DCIVIL_CLOCK_ID="civil-test"']+(['-DCIVIL_NATIVE_UTC'] if native else ['-DPORTABLE_RTC_UTC8_DENVER'])
   subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(inc),*flags,*map(str,sources),'-o',str(exe)],check=True)
   for mode in ('read','loss'):subprocess.run([str(exe),mode],check=True)
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);a=p.parse_args();run(a.runtime.resolve())

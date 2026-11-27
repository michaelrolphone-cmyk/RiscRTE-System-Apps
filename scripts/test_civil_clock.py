#!/usr/bin/env python3
import argparse,os,shutil,subprocess,tempfile
from pathlib import Path
from scene_sdk import stage_sdk
ROOT=Path(__file__).resolve().parents[1]
def run(runtime,utilities=None):
 with tempfile.TemporaryDirectory(prefix='civil-test-') as tmp:
  out=Path(tmp);inc=stage_sdk(runtime,ROOT,out)
  for name in ('PortableTimeZone.h','PortableTimeZonePreference.h','PortableRtcClock.h','PortableTime.h','PortableRtcBasis.h'):shutil.copyfile(ROOT/'lib/PortableApps/include'/name,inc/name)
  shutil.copytree(ROOT/'lib/PortableApps/time',out/'time')
  for native in (False,True):
   for test in ('clock','policy'):
    for sanitize in (False,True):
     sources=[ROOT/'Services/civil_clock/clock.c',ROOT/'test/civil_clock'/f'{test}_test.c',*[ROOT/'lib/PortableApps/src'/n for n in ('PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c')]]
     exe=out/(('utc' if native else 'raw')+test+str(sanitize));flags=['-DCIVIL_CLOCK_ID="civil-test"']+(['-DCIVIL_NATIVE_UTC'] if native else ['-DPORTABLE_RTC_UTC8_DENVER'])
     if sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all']
     subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-I'+str(inc),*flags,*map(str,sources),'-o',str(exe)],check=True)
     for mode in (('read','loss') if test=='clock' else (None,)):
      subprocess.run([str(exe)]+([mode] if mode else []),check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))

  if utilities is not None:
   for sanitized in (False,True):
    flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
    exe=out/('retained-rule-'+str(sanitized))
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,
      '-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(utilities/'lib/Alarm/include'),
      str(ROOT/'test/civil_clock/retained_rule_test.c'),
      str(ROOT/'lib/PortableApps/src/PortableTimeZone.c'),str(ROOT/'lib/PortableApps/src/PortableTimeZoneCatalog.c'),
      '-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))

if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--utilities',type=Path);a=p.parse_args();run(a.runtime.resolve(),a.utilities.resolve() if a.utilities else None)

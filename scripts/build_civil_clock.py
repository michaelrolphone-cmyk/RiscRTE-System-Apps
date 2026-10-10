#!/usr/bin/env python3
"""Build the read-only local calendar policy for both installed clock domains."""
import argparse,json,shutil,tempfile,os,subprocess
from pathlib import Path
from scene_sdk import stage_sdk
from scene_build import build,compiler_path,json_write
ROOT=Path(__file__).resolve().parents[1]
def run(runtime,output):
 if output.exists():raise FileExistsError('Choose a new output directory')
 output.mkdir(parents=True)
 with tempfile.TemporaryDirectory(prefix='civil-clock-') as tmp:
  include=stage_sdk(runtime,ROOT,Path(tmp))
  for name in ('PortableTimeZone.h','PortableTimeZonePreference.h','PortableRtcClock.h','PortableTime.h'):
   shutil.copyfile(ROOT/'lib/PortableApps/include'/name,include/name)
  shutil.copytree(ROOT/'lib/PortableApps/time',Path(tmp)/'time')
  compiler=compiler_path();receipts=[]
  for target in ('watch','x4'):
   sources=[ROOT/'Services/civil_clock/clock.c',ROOT/'lib/PortableApps/src/PortableTimeZone.c',ROOT/'lib/PortableApps/src/PortableTimeZoneCatalog.c']
   flags=[f'-DCIVIL_CLOCK_ID="civil-clock-{target}"']
   if target=='x4':
    sources += [ROOT/'lib/PortableApps/src'/name for name in ('PortableTimeZonePreference.c',)]
    flags+=['-DCIVIL_NATIVE_UTC']
   else:flags+=['-DPORTABLE_RTC_UTC8_DENVER']
   manifest=json.loads((ROOT/'Services/civil_clock'/f'{target}.json').read_text())
   receipts.append(build(compiler,include,output/target,manifest,sources,flags))
  validator=Path(tmp)/'validate-elf'
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'),str(ROOT/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
  for elf in output.rglob('*.elf'):subprocess.run([str(validator),str(elf)],check=True)
  json_write(output/'packages.json' ,{'schema':1,'packages':receipts})
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
 a=p.parse_args();run(a.runtime.resolve(),a.output.resolve())

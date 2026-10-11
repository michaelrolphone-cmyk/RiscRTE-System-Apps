#!/usr/bin/env python3
"""Build the one text session host plus shared scene presenter/profiles."""
import argparse,json,os,shutil,subprocess,tempfile
from pathlib import Path
from build_scene_services import run as build_scenes
from scene_build import build,compiler_path,json_write
from scene_sdk import stage_sdk
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();runtime=a.runtime.resolve();out=a.output.resolve()
if out.exists():raise FileExistsError(out)
out.mkdir(parents=True);build_scenes(runtime,out/'presentation')
with tempfile.TemporaryDirectory(prefix='text-input-sdk-') as temporary:
 inc=stage_sdk(runtime,ROOT,Path(temporary));shutil.copy(ROOT/'sdk/app/RiscTextEntryV1.h',inc)
 for name in ['RiscUsbHidV1.h','RiscUsbControllerV1.h','RiscUsbProviderV1.h']:
  destination=inc/name;source=ROOT/'Services/text_input'/name
  if destination.exists() and destination.read_bytes()!=source.read_bytes():raise ValueError(f'USB header mismatch: {name}')
  shutil.copy(source,destination)
 manifest=json.loads((ROOT/'Services/text_input/manifest.json').read_text())
 receipt=build(compiler_path(),inc,out/'text-input-host',manifest,[ROOT/'Services/text_input/host.c'])
 # Alternative dependency composition; same code and ID, never load both.
 shutil.copy(ROOT/'Services/text_input/manifest-usb.json',out/'text-input-host/manifest-usb.json')
 validator=Path(temporary)/'validate'
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'),str(ROOT/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
 subprocess.run([str(validator),str(out/'text-input-host/driver.elf')],check=True)
 json_write(out/'text-input.json',{'schema':1,'package':receipt,'deployment':'unbound; select exactly one manifest, base or USB dependency composition','physical_testing':'not performed'})

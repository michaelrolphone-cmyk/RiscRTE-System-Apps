#!/usr/bin/env python3
"""Clean-build only the recovered text0.1.2 provider; scene is composed separately."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
from scene_sdk import stage_sdk
from scene_build import build,compiler_path,json_write
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
out=a.output.resolve()
if out.exists():raise FileExistsError(out)
out.mkdir(parents=True);inc=stage_sdk(a.runtime.resolve(),ROOT,out/'sdk')
shutil.copyfile(ROOT/'sdk/app/RiscTextEntryV1.h',inc/'RiscTextEntryV1.h')
for name in ('RiscUsbHidV1.h','RiscUsbControllerV1.h','RiscUsbProviderV1.h'):
 source=ROOT/'Services/text_input'/name;dest=inc/name
 if dest.exists():assert dest.read_bytes()==source.read_bytes(),name
 shutil.copyfile(source,dest)
manifest=json.loads((ROOT/'Services/text_input/manifest.json').read_text());assert manifest['version']=='0.1.2'
receipt=build(compiler_path(),inc,out/'text-input-host',manifest,[ROOT/'Services/text_input/host.c'])
shutil.copyfile(ROOT/'Services/text_input/manifest-usb.json',out/'text-input-host/manifest-usb.json')
validator=out/'validate'
subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'),str(ROOT/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
subprocess.run([str(validator),str(out/'text-input-host/driver.elf')],check=True)
receipt['source_revision']=subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip()
receipt['clean_source_build']=True;receipt['prior_product_binary_inputs']=[]
receipt['source_sha256'].update({str(ROOT/name):hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in ['Services/text_input/manifest.json','Services/text_input/manifest-usb.json','sdk/app/RiscTextEntryV1.h','scripts/build_text_home_service.py']})
json_write(out/'text-input-host/build.json',receipt)
print('Text0.1.2 clean source build and strict loader validation PASS')

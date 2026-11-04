#!/usr/bin/env python3
"""Repeat-build explicit semantic settings.policy provider; never install it."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
from build_settings_scene import stage
from scene_build import build,compiler_path,json_write
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--runtime-sdk',type=Path,required=True);p.add_argument('--driver-sdk',type=Path,required=True);p.add_argument('--utilities',type=Path,required=True);p.add_argument('--policy',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
inc=stage(a.runtime_sdk.resolve(),a.utilities.resolve(),out)
for name in ['RiscProviderV2.h','RiscStreamProviderV1.h']:
 source=a.driver_sdk/name
 if not source.is_file():p.error('Missing canonical driver SDK '+str(source))
 shutil.copyfile(source,inc/name)
 origins=json.loads((out/'header-origins.json').read_text());origins[name]={'path':str(source.resolve()),'sha256':hashlib.sha256(source.read_bytes()).hexdigest()};json_write(out/'header-origins.json',origins)
subprocess.run(['python3',str(ROOT/'scripts/generate_shared_settings_policy.py'),str(a.policy),str(inc)],check=True)
manifest={'type':'driver','id':'settings-policy','version':'1.0.0','architecture':'xtensa-esp32s3','file_name':'settings_policy.elf','entry':'t5_driver_get','provides':[{'capability':'settings.policy','api':1}],'requires':[]}
flags=['-DALARM_SERVICE_TAGGED_V2','-DALARM_SERVICE_CLOCK_EDIT_V2','-DPORTABLE_PREFERENCE_ANY_STORE','-DPORTABLE_DESK_POINTS_FACE']
receipts=[build(compiler_path(),inc,out/name,manifest,[ROOT/'Services/settings_policy/service.c'],flags) for name in ['first','repeat']]
assert receipts[0]['sha256']==receipts[1]['sha256']
validator=out/'validate';subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'),str(ROOT/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
subprocess.run([str(validator),str(out/'first/settings_policy.elf')],check=True)
json_write(out/'qualification.json',{'repeat_identical':True,'elf_sha256':receipts[0]['sha256'],'policy':json.loads(a.policy.read_text()),'policy_sha256':hashlib.sha256(a.policy.read_bytes()).hexdigest(),'hardware_tested':False,'deployment_composed':False,'driver_sdk_sha256':{n:hashlib.sha256((a.driver_sdk/n).read_bytes()).hexdigest() for n in ['RiscProviderV2.h','RiscStreamProviderV1.h']}})
print(receipts[0]['sha256'])

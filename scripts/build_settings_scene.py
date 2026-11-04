#!/usr/bin/env python3
"""Build a single device-neutral Settings ELF from explicit source dependencies.

No product/profile switch is accepted. settings.policy and ui.scene providers
are composed separately. This does not install firmware or claim hardware QA.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import os
from scene_build import build, compiler_path, json_write
ROOT = Path(__file__).resolve().parents[1]

def stage(runtime_sdk, utilities, output):
    include = output / 'include'
    include.mkdir(parents=True, exist_ok=True)
    origins = {}
    for directory, replace in ((ROOT/'lib/PortableApps/include', True),
                               (ROOT/'sdk/app', False), (runtime_sdk, True),
                               (utilities/'lib/Alarm/include', True)):
        for source in sorted(directory.glob('*.h')):
            if replace or source.name not in origins:
                origins[source.name] = source
    required = ['RiscRuntimeV1.h', 'RiscRealtimeV1.h', 'RiscResidentShellV1.h']
    for name in required:
        if origins.get(name) != runtime_sdk/name:
            raise ValueError('Missing canonical Runtime SDK header: '+name)
    for name, source in origins.items():
        shutil.copyfile(source, include/name)
    shutil.copytree(ROOT/'lib/PortableApps/time', output/'time', dirs_exist_ok=True)
    json_write(output/'header-origins.json', {name:{'path':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for name,p in origins.items()})
    return include

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--runtime-sdk',type=Path,required=True)
    parser.add_argument('--utilities',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    output=args.output.resolve();output.mkdir(parents=True,exist_ok=True)
    (output/'qualification.json').unlink(missing_ok=True)
    include=stage(args.runtime_sdk.resolve(),args.utilities.resolve(),output)
    names=['PortableSettingsModel','PortableSettingsRuntime','PortableSettingsScene',
           'PortableSetTime','PortableRealtimeClient','PortableTimeZone',
           'PortableTimeZoneCatalog','PortableTimeZonePreference']
    sources=[ROOT/'Apps/settings_scene.c',*[ROOT/f'lib/PortableApps/src/{n}.c' for n in names],
             ROOT/'lib/NativeApps/src/UnsignedDivisionCompat.c',ROOT/'lib/NativeApps/src/SignedDivisionCompat.c']
    manifest={'type':'application','id':'settings','version':'2.0.0',
              'architecture':'xtensa-esp32s3','file_name':'settings.elf',
              'entry':'app_main','display_name':'Settings','runtime_profile':'shared-scene-v1',
              'requires':[{'capability':'ui.scene','api':1},{'capability':'settings.policy','api':1}],
              'optional':[{'capability':name,'api':version} for name,version in [
                  ('storage.key-value',1),('storage.key-value.atomic',1),
                  ('storage.preferences',1),('storage.key-value.availability',1),
                  ('runtime.realtime',1),('rtc.clock',2),('alarm.service',2)]]}
    definitions=['-DALARM_SERVICE_TAGGED_V2','-DALARM_SERVICE_CLOCK_EDIT_V2','-DPORTABLE_SETTINGS_ATOMIC_EXPERIMENTAL',
                 '-DPORTABLE_DESK_POINTS_FACE',
                 '-DPORTABLE_PREFERENCE_ANY_STORE','-Wno-misleading-indentation']
    cc=compiler_path()
    receipts=[build(cc,include,output/directory,manifest,sources,definitions,
        {'app_main','risc_resident_app_descriptor_v1'},normalize_relocations=True)
        for directory in ('first','repeat')]
    if receipts[0]['sha256']!=receipts[1]['sha256']:
        raise ValueError('Repeat builds differ')
    validator=output/'validate'
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',
        '-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),
        str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'),str(ROOT/'test/native_apps/validate_test.c'),
        '-o',str(validator)],check=True)
    subprocess.run([str(validator),str(output/'first/settings.elf')],check=True)
    json_write(output/'qualification.json',{'repeat_identical':True,'sha256':receipts[0]['sha256'],
        'hardware_tested':False,'deployment_composed':False,'device_specific_definitions':[],
        'scope':'One target ELF; provider/deployment and physical acceptance remain separate.'})
    print(json.dumps(receipts[0],indent=2))
if __name__=='__main__':main()

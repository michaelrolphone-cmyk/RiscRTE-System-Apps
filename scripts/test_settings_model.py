#!/usr/bin/env python3
"""Production device-neutral Settings model with transport/fault-injection doubles.
A caller-supplied Runtime SDK is required; no mutable/private SDK is selected.
"""
import argparse, os, shutil, subprocess, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--runtime-sdk',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
assert (a.runtime_sdk/'RiscRealtimeV1.h').is_file()
with tempfile.TemporaryDirectory(prefix='settings-model-') as temporary:
 out=Path(temporary)/'test'
 include=Path(temporary)/'include'
 shutil.copytree(ROOT/'lib/PortableApps/include',include)
 shutil.copytree(ROOT/'lib/PortableApps/time',Path(temporary)/'time')
 for header in a.runtime_sdk.glob('*.h'):shutil.copyfile(header,include/header.name)
 sources=['test/native_apps/settings_model_test.c',*[f'lib/PortableApps/src/{n}.c' for n in ['PortableSettingsModel','PortableTimeZone','PortableTimeZoneCatalog','PortableTimeZonePreference','PortableSetTime']]]
 command=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-I'+str(include),*[str(ROOT/s) for s in sources],'-Wl,--gc-sections','-o',str(out)]
 if a.sanitize:command[1:1]=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
 subprocess.run(command,check=True);subprocess.run([str(out)],check=True)

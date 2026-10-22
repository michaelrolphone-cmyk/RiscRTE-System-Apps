#!/usr/bin/env python3
"""Qualify emitted Home optional metadata with a selected read-only Runtime.

All compilation and fixture metadata stay in a temporary directory. The real
Runtime admission, app entry and remote_battery.c execute; provider modules are
host-only stubs and do not qualify RF, rendering or a target ELF.
"""
import argparse
import ast
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

import portable_quick_build

ROOT=Path(__file__).resolve().parents[1]
RUNTIME_REVISION='13eca114cb58469cf6004fafc3da84fbd9d29beb'
RUNTIME_SOURCES=(
    'src/bootstrap/Json.cpp','src/bootstrap/Board.cpp','src/bootstrap/Runtime.cpp',
    'src/runtime/streams/AppStreamSessions.cpp','src/runtime/streams/ProviderQueueHost.cpp',
    'src/runtime/drivers/ProviderGraphV2.cpp','src/runtime/drivers/ProviderModuleV2.cpp')
SYSTEM_SOURCES=(
    'scripts/portable_quick_build.py','scripts/build_paper_clock.py','scripts/test_quick_remote_manifest.py',
    'test/native_apps/quick_remote_manifest_test.cpp','lib/PortableApps/src/remote_battery.c',
    'lib/PortableApps/include/PortableRemoteBattery.h','lib/PortableApps/include/RiscBluetoothSensorsV1.h',
    'lib/PortableApps/include/RiscTelemetryV1.h')


def hashes(root,paths):
    return {path:hashlib.sha256((root/path).read_bytes()).hexdigest() for path in paths}


def emitted_manifest():
    # Execute the builder's actual declaration/manifest emission region. The
    # fixture selects the non-sparse resident host; required rows are not erased.
    tree=ast.parse((ROOT/'scripts/build_paper_clock.py').read_text())
    build=next(node for node in tree.body if isinstance(node,ast.FunctionDef) and node.name=='build')
    start=next(i for i,node in enumerate(build.body) if isinstance(node,ast.Assign)
               and ast.unparse(node.targets[0])=='needs')
    end=next(i for i in range(start,len(build.body)) if isinstance(build.body[i],ast.Expr)
             and 'write_text' in ast.unparse(build.body[i]))
    a=argparse.Namespace(navigation=False,sleep_capability=None,desk_clock=False,alarm_client=True,
                         quick_actions=True,quick_radios=True,quick_remote_battery=True,
                         quick_remote_battery_version='0.4.21',crash_report_sd=False,
                         experimental_atomic_quick=False,experimental_settings_availability=False,
                         installed_catalog_volume_instance=None)
    values={'a':a,'version':'0.4.21','tagged_alarm':False,'portable_quick_build':portable_quick_build}
    exec(compile(ast.Module(body=build.body[start:end],type_ignores=[]),'<actual Home metadata emission>','exec'),values)
    manifest=values['manifest']
    assert manifest['optional']==[{'capability':'bluetooth.sensors','api':1}]
    assert all(row['capability']!='bluetooth.sensors' for row in manifest['requires'])
    return manifest


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime-repo',type=Path,required=True)
    parser.add_argument('--sanitize',action='store_true')
    args=parser.parse_args();runtime=args.runtime_repo.resolve()
    commit=subprocess.check_output(['git','-C',str(runtime),'rev-parse','HEAD'],text=True).strip()
    if commit!=RUNTIME_REVISION:parser.error('This qualification requires the explicitly selected Runtime '+RUNTIME_REVISION)
    checked=(*RUNTIME_SOURCES,'src/bootstrap/AppDeclarations.h','src/bootstrap/Runtime.h',
             'src/runtime/packages/PackageIdentity.h')
    subprocess.run(['git','-C',str(runtime),'diff','--exit-code','HEAD','--',*checked],check=True)
    runtime_hashes=hashes(runtime,checked);system_hashes=hashes(ROOT,SYSTEM_SOURCES)
    manifest=emitted_manifest()
    fixture=ROOT/'test/native_apps/quick_remote_manifest_test.cpp'
    san=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-g'] if args.sanitize else []
    cc=os.environ.get('CC','cc');cxx=os.environ.get('CXX','c++')
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')
    with tempfile.TemporaryDirectory(prefix='t061-runtime-optional-') as directory:
        out=Path(directory);includes=out/'include';includes.mkdir()
        for header in ('PortableRemoteBattery.h','RiscBluetoothSensorsV1.h','RiscTelemetryV1.h'):
            shutil.copyfile(ROOT/'lib/PortableApps/include'/header,includes/header)
        include_flags=['-I'+str(includes),'-I'+str(runtime/'src'),'-I'+str(runtime/'sdk/app'),
                       '-I'+str(runtime/'sdk/driver'),'-I'+str(runtime/'sdk/hardware'),
                       '-I'+str(runtime/'lib/ArduinoJson/src'),'-I'+str(runtime/'test'),
                       '-I'+str(runtime/'test/drivers/stubs')]
        shared=['-Wl,-undefined,dynamic_lookup'] if sys.platform=='darwin' else []
        common=[cxx,*san,'-std=c++17','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers',*include_flags]
        subprocess.run([*common,'-fPIC','-fvisibility=hidden','-shared',*shared,
                        '-DQUICK_MANIFEST_APP',str(fixture),'-o',str(out/'default.elf')],check=True)
        (out/'template.json').write_text(json.dumps(manifest,indent=2)+'\n')
        providers=[(f'provider-{index}',row) for index,row in enumerate(manifest['requires'])
                   if row['capability']!='storage.key-value']
        providers.append(('sensors',manifest['optional'][0]))
        for identifier,row in providers:
            provider={'type':'driver','id':identifier,'version':'1.0.0','driver_abi':2,
                      'architecture':'xtensa-esp32s3','file_name':identifier+'.elf','requires':[],
                      'provides':[row]}
            (out/(identifier+'.json')).write_text(json.dumps(provider)+'\n')
            flags=['-DQUICK_MANIFEST_SENSOR'] if identifier=='sensors' else []
            subprocess.run([*common,'-fPIC','-fvisibility=hidden','-shared',*shared,
                            '-DQUICK_MANIFEST_PROVIDER',*flags,'-DTEST_PROVIDER_ID="'+identifier+'"',
                            '-DTEST_CAPABILITY="'+row['capability']+'"','-DTEST_API='+str(row['api']),
                            str(fixture),'-o',str(out/(identifier+'.elf'))],check=True)
        component=out/'remote_battery.o'
        subprocess.run([cc,*san,'-std=c11','-Wall','-Wextra','-Werror',*include_flags,'-c',
                        str(ROOT/'lib/PortableApps/src/remote_battery.c'),'-o',str(component)],check=True)
        no_pie=['-no-pie'] if args.sanitize and sys.platform!='darwin' else []
        executable=out/'qualify'
        subprocess.run([*common,*no_pie,'-rdynamic',*[str(runtime/path) for path in RUNTIME_SOURCES],
                        str(fixture),str(component),'-ldl','-o',str(executable)],check=True)
        subprocess.run([str(executable),str(out)],env=env,check=True)
    if runtime_hashes!=hashes(runtime,checked) or system_hashes!=hashes(ROOT,SYSTEM_SOURCES):
        raise RuntimeError('Qualification inputs changed during the run; rerun against settled sources')
    print(json.dumps({'runtime_commit':commit,'runtime_sha256':runtime_hashes,
        'system_sha256':system_hashes,
        'manifest':manifest,'sanitize':args.sanitize,'hardware_verified':False},sort_keys=True))
    return 0


if __name__=='__main__':sys.exit(main())

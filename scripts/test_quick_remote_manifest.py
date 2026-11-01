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
import shlex

import portable_quick_build

ROOT=Path(__file__).resolve().parents[1]
RUNTIME_REVISION='6a3b3d46f5b2857eacc6e9863799dec1f466b036'
RUNTIME_SOURCES=(
    'src/bootstrap/Json.cpp','src/bootstrap/Board.cpp','src/bootstrap/Runtime.cpp',
    'src/runtime/streams/AppStreamSessions.cpp','src/runtime/streams/ProviderQueueHost.cpp',
    'src/runtime/drivers/ProviderGraphV2.cpp','src/runtime/drivers/ProviderModuleV2.cpp')
SYSTEM_SOURCES=(
    'scripts/portable_quick_build.py','scripts/build_paper_clock.py','scripts/test_quick_remote_manifest.py',
    'test/native_apps/quick_remote_manifest_test.cpp','lib/PortableApps/src/remote_battery.c',
    'lib/PortableApps/include/PortableRemoteBattery.h','lib/PortableApps/include/RiscBluetoothSensorsV1.h',
    'lib/PortableApps/include/RiscTelemetryV1.h','lib/PortableApps/include/PortableRemoteBatteryLink.h',
    'lib/PortableApps/include/RiscBluetoothSensorLinkV1.h')


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
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--runtime-tree',type=Path,required=True,help='Exact public Git tree JSON at the selected Runtime revision')
    args=parser.parse_args();runtime=args.runtime_repo.resolve()
    tree=json.loads(args.runtime_tree.read_text());assert tree['sha']==RUNTIME_REVISION and not tree.get('truncated')
    blobs={x['path']:x['sha'] for x in tree['tree'] if x['type']=='blob'}
    commit=RUNTIME_REVISION
    checked=(*RUNTIME_SOURCES,'src/bootstrap/AppDeclarations.h','src/bootstrap/Runtime.h',
             'src/runtime/packages/PackageIdentity.h')
    def verify_runtime(path):
        path=Path(path).resolve()
        if path.is_relative_to(runtime):
            name=path.relative_to(runtime).as_posix();data=path.read_bytes()
            assert hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()==blobs[name],name
    for name in checked:verify_runtime(runtime/name)
    runtime_hashes=hashes(runtime,checked);system_hashes=hashes(ROOT,SYSTEM_SOURCES)
    manifest=emitted_manifest()
    fixture=ROOT/'test/native_apps/quick_remote_manifest_test.cpp'
    san=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-g'] if args.sanitize else []
    cc=os.environ.get('CC','cc');cxx=os.environ.get('CXX','c++')
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')
    commands=[];inputs={};dep_index=0
    def run(command,**kwargs):
        nonlocal dep_index
        sources=[Path(x) for x in command if isinstance(x,str) and x.endswith(('.c','.cpp'))]
        flags=[];skip=False
        for x in command[1:]:
            if skip:skip=False;continue
            if x=='-o':skip=True;continue
            if x in ('-c','-shared','-rdynamic','-ldl','-no-pie') or x.startswith('-Wl,') or x.endswith(('.c','.cpp','.o')):continue
            flags.append(x)
        for source in sources:
            dep=out/('input-'+str(dep_index)+'.d');dep_index+=1
            probe=[command[0],*flags,'-MM','-MF',str(dep),str(source)]
            commands.append(probe);subprocess.run(probe,check=True)
            for word in shlex.split(dep.read_text().replace('\\\n',' ').split(':',1)[1]):
                path=Path(word).resolve();verify_runtime(path);digest=hashlib.sha256(path.read_bytes()).hexdigest()
                assert str(path) not in inputs or inputs[str(path)]==digest
                inputs[str(path)]=digest
        commands.append(command);return subprocess.run(command,**kwargs)
    if True:
        out=args.output.resolve();out.mkdir(parents=True,exist_ok=True);includes=out/'include';includes.mkdir(exist_ok=True)
        for header in ('PortableRemoteBattery.h','PortableRemoteBatteryLink.h','RiscBluetoothSensorLinkV1.h','RiscBluetoothSensorsV1.h','RiscTelemetryV1.h'):
            shutil.copyfile(ROOT/'lib/PortableApps/include'/header,includes/header)
        include_flags=['-I'+str(includes),'-I'+str(runtime/'src'),'-I'+str(runtime/'sdk/app'),
                       '-I'+str(runtime/'sdk/driver'),'-I'+str(runtime/'sdk/hardware'),
                       '-I'+str(runtime/'lib/ArduinoJson/src'),'-I'+str(runtime/'test'),
                       '-I'+str(runtime/'test/drivers/stubs')]
        shared=['-Wl,-undefined,dynamic_lookup'] if sys.platform=='darwin' else []
        common=[cxx,*san,'-std=c++17','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers',*include_flags]
        run([*common,'-fPIC','-fvisibility=hidden','-shared',*shared,
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
            run([*common,'-fPIC','-fvisibility=hidden','-shared',*shared,
                            '-DQUICK_MANIFEST_PROVIDER',*flags,'-DTEST_PROVIDER_ID="'+identifier+'"',
                            '-DTEST_CAPABILITY="'+row['capability']+'"','-DTEST_API='+str(row['api']),
                            str(fixture),'-o',str(out/(identifier+'.elf'))],check=True)
        component=out/'remote_battery.o'
        run([cc,*san,'-std=c11','-Wall','-Wextra','-Werror',*include_flags,'-c',
                        str(ROOT/'lib/PortableApps/src/remote_battery.c'),'-o',str(component)],check=True)
        no_pie=['-no-pie'] if args.sanitize and sys.platform!='darwin' else []
        executable=out/'qualify'
        run([*common,*no_pie,'-rdynamic',*[str(runtime/path) for path in RUNTIME_SOURCES],
                        str(fixture),str(component),'-ldl','-o',str(executable)],check=True)
        result=subprocess.run([str(executable),str(out)],env=env,check=True,capture_output=True,text=True)
        (out/'result.txt').write_text(result.stdout+result.stderr);print(result.stdout,flush=True)
    if runtime_hashes!=hashes(runtime,checked) or system_hashes!=hashes(ROOT,SYSTEM_SOURCES):
        raise RuntimeError('Qualification inputs changed during the run; rerun against settled sources')
    assert all(hashlib.sha256(Path(path).read_bytes()).hexdigest()==digest for path,digest in inputs.items())
    receipt={'runtime_tree_sha256':hashlib.sha256(args.runtime_tree.read_bytes()).hexdigest(),'toolchains':{name:{'path':shutil.which(name),'sha256':hashlib.sha256(Path(shutil.which(name)).resolve().read_bytes()).hexdigest(),'version':subprocess.check_output([name,'--version'],text=True).splitlines()[0]} for name in (cc,cxx)},'inputs':inputs,'commands':commands,'binaries':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in out.iterdir() if p.suffix=='.elf' or p.name=='qualify'},'runtime_commit':commit,'runtime_sha256':runtime_hashes,
        'system_sha256':system_hashes,
        'manifest':manifest,'sanitize':args.sanitize,'hardware_verified':False,'scope':'11 actual Runtime optional admission checks and four app entries; provider endpoints doubled, no RF/render/physical storage'}
    (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(json.dumps({'checks':11,'entries':4,'receipt':str(out/'receipt.json')},sort_keys=True))
    return 0


if __name__=='__main__':sys.exit(main())

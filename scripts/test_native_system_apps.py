#!/usr/bin/env python3
"""Qualify actual native System Apps, selected SDKs and preserved legacy ELFs."""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
import tarfile
import tempfile
import portable_native_toolbar_build as native

ROOT = Path(__file__).resolve().parents[1]
BASE = 'd305e6b'
CASES = ['home', 'home-refused', 'utc', 'zone', 'missing-zone', 'native-unset',
         'quick', 'fini-owned', 'fini-refused', 'kv-context', 'native-release-false', 'alarm-retained']
APPS = {'springboard': ('springboard', []),
        'file_browser': ('file_browser', ['--storage-capability','storage.volume','--storage-instance','11']),
        'wifi_settings': ('wifi', ['--wifi-instance','15','--return-app','springboard.elf'])}
HELPERS = [ROOT/p for p in native.SOURCES]
QUICK = [ROOT/'lib/PortableApps/src'/name for name in ['quick_actions.c','quick_render.c','quick_session.c']]


def run(command, **kwargs):
    return subprocess.run(list(map(str, command)), check=True, **kwargs)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--utilities', type=Path, required=True)
    parser.add_argument('--xtensa-cc', type=Path, default=ROOT.parent/'watch-build-tools/platformio-core/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc')
    parser.add_argument('--output-dir', type=Path, default=ROOT/'build/native-system-apps')
    parser.add_argument('--skip-legacy', action='store_true')
    args = parser.parse_args();out=args.output_dir.resolve();out.mkdir(parents=True, exist_ok=True)
    env=dict(os.environ,NATIVE_APP_CC=str(args.xtensa_cc.resolve()),ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    compiler=subprocess.check_output([args.xtensa_cc,'--version'],text=True).splitlines()[0]
    assert '8.4.0' in compiler and '2021r2-patch5' in compiler,compiler
    receipt={'compiler':compiler,'source_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
             'base_commit':BASE,'native':{},'host':{},'legacy':{},'hardware':'not run','publication':'none'}
    for app,(builder,extra) in APPS.items():
        product=out/app
        run(['python',ROOT/'scripts'/('build_portable_'+builder+'.py'), '--time-profile','x4-native-time',
             '--native-time-runtime-repo',args.runtime.resolve(),'--tagged-alarm-utilities',args.utilities.resolve(),
             '--alarm-client','--quick-actions','--home-app','default.elf','--output-dir',product,*extra],env=env)
        manifest=json.loads((product/(app+'.json')).read_text());record=json.loads((product/(app+'-build-record.json')).read_text())
        admission=json.loads((product/'x4-native-app.json').read_text())
        assert admission['alarm_api']==2 and admission['requires']==manifest['requires']
        assert admission['elf_sha256']==sha(product/(app+'.elf'))
        assert admission['elf_bytes']==(product/(app+'.elf')).stat().st_size
        assert admission['source_revision']==record['repository_commit']
        assert set(admission['sdk_sha256'])==set(native.SDK_HEADERS+native.portable_alarm_build.HEADERS)
        needs={(r['capability'],r['api']) for r in manifest['requires']}
        assert {('alarm.service',2),('runtime.realtime',1),('storage.key-value',1)}<=needs
        assert not any(name in ('rtc.clock','runtime.realtime-control') for name,_ in needs)
        assert ('alarm.service',1) not in needs
        grants={(g['capability'],g['api'],g['instance_id']) for g in record['required_grants']}
        assert ('runtime.realtime',1,0) in grants and ('storage.key-value',1,1) in grants
        include=product/'native-time-sdk/include'
        for name in native.SDK_HEADERS:
            expected=subprocess.check_output(['git','-C',args.runtime,'show',native.RUNTIME_COMMIT+':sdk/app/'+name])
            assert (include/name).read_bytes()==expected
        for name in native.portable_alarm_build.HEADERS:
            expected=subprocess.check_output(['git','-C',args.utilities,'show',native.portable_alarm_build.UTILITIES_COMMIT+':lib/Alarm/include/'+name])
            assert (include/name).read_bytes()==expected
        receipt['native'][app]={'version':manifest['version'],'elf_sha256':sha(product/(app+'.elf')),
                                'required_grants':record['required_grants'],'sdk':record['native_time_sdk'],
                                'alarm_sdk':record['tagged_alarm_sdk'],'imports':record['imports'],'exports':record['exports']}
        plain=out/(app+'-no-quick')
        run(['python',ROOT/'scripts'/('build_portable_'+builder+'.py'),'--time-profile','x4-native-time',
             '--native-time-runtime-repo',args.runtime.resolve(),'--tagged-alarm-utilities',args.utilities.resolve(),
             '--alarm-client','--home-app','default.elf','--output-dir',plain,*extra],env=env)
        plain_manifest=json.loads((plain/(app+'.json')).read_text())
        assert not any(item['capability'] in ('rtc.clock','runtime.realtime-control') for item in plain_manifest['requires'])
        receipt['native'][app+'-no-quick']={'version':plain_manifest['version'],'elf_sha256':sha(plain/(app+'.elf'))}
        flags=['-DPORTABLE_NATIVE_TIME_TOOLBAR','-DPORTABLE_NATIVE_CUSTODY_FENCE','-DALARM_SERVICE_TAGGED_V2',
               '-DTEST_NATIVE_TOOLBAR_QUICK','-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_ALARM_CLIENT',
               '-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_HOME_APP="default.elf"']
        if app=='file_browser':flags+=['-DPORTABLE_FILE_BROWSER_APP','-DPORTABLE_NOVA_UI','-DPORTABLE_APP_OWNS_TOUCH_CHROME','-DPORTABLE_FORCE_FULL_FRAMES','-DPORTABLE_FILE_BROWSER_CAPABILITY="storage.volume"','-DPORTABLE_FILE_BROWSER_INSTANCE=11u','-DFILE_BROWSER_RETURN_APP="springboard.elf"']
        if app=='wifi_settings':flags+=['-DPORTABLE_WIFI_SETTINGS_APP','-DPORTABLE_WIFI_INSTANCE=15u','-DPORTABLE_WIFI_STORAGE_INSTANCE=6','-DWIFI_RETURN_APP="springboard.elf"']
        for sanitized in (False, True):
            label=app+('-asan-ubsan' if sanitized else '-normal');binary=out/label
            san=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
            run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function',*san,*flags,
                 '-I'+str(include),'-I'+str(ROOT/'lib/NativeApps/include'),ROOT/'test/native_apps/native_system_apps_test.c',
                 ROOT/'test/native_apps/native_system_app_entry.c',*HELPERS,*QUICK,'-Wl,--wrap=free','-o',binary])
            cases=[]
            with (out/(label+'.log')).open('w') as log:
                for case in CASES:
                    result=run([binary,case],env=env,text=True,stdout=subprocess.PIPE,timeout=20)
                    log.write(result.stdout);cases.append(json.loads(result.stdout))
            receipt['host'][label]=cases
            print(label+': '+str(len(cases))+' actual app/adapter/owner cases passed',flush=True)
    if not args.skip_legacy:
        with tempfile.TemporaryDirectory(prefix='native-system-legacy-') as temporary:
            baseline=Path(temporary)/'baseline';baseline.mkdir()
            archive=subprocess.check_output(['git','-C',ROOT,'archive',BASE])
            with tarfile.open(fileobj=io.BytesIO(archive)) as tar:tar.extractall(baseline,filter='data')
            run(['git','-C',baseline,'init','-q'])
            run(['git','-C',baseline,'add','.'])
            run(['git','-C',baseline,'-c','user.name=Fixture','-c','user.email=fixture@localhost','commit','-qm','Baseline source snapshot'])
            for app,(builder,extra) in APPS.items():
                for profile in ['watch','paper']:
                    opts=['--alarm-client','--quick-actions','--home-app','default.elf',*extra]
                    if profile=='paper':opts+=['--display-rotation','90','--navigation','--wall-time']
                    elif app=='springboard':opts+=['--denver']
                    elif app=='file_browser':opts+=['--quick-controls']
                    else:opts+=['--nova-ui']
                    products=[]
                    for label,repo in [('base',baseline),('current',ROOT)]:
                        product=out/('legacy-'+app+'-'+profile+'-'+label);products.append(product)
                        run(['python',repo/'scripts'/('build_portable_'+builder+'.py'),*opts,'--output-dir',product],env=env)
                    for extension in ['.elf','.json']:
                        a,b=[p/(app+extension) for p in products];assert a.read_bytes()==b.read_bytes(),(app,profile,extension)
                    receipt['legacy'][app+'-'+profile]=sha(products[1]/(app+'.elf'))
    receipt['source_sha256']={str(path.relative_to(ROOT)):sha(path) for path in
        [Path(__file__).resolve(),ROOT/'test/native_apps/native_system_apps_test.c',ROOT/'test/native_apps/native_system_app_entry.c',
         ROOT/'lib/PortableApps/src/adapter.c',ROOT/'scripts/portable_native_toolbar_build.py',*HELPERS]}
    (out/'evidence.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print('Native System Apps qualification complete: '+str(out/'evidence.json'))

if __name__=='__main__':main()

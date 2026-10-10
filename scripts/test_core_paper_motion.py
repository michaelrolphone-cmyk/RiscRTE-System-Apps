#!/usr/bin/env python3
"""Core paper motion through actual app loops, pinned target SDKs and gestures.

Provider timing is simulated. This does not qualify physical panel motion.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import portable_native_toolbar_build as native
import build_portable_settings as settings
import portable_paper_build as paper

ROOT=Path(__file__).resolve().parents[1]
CASES=['repeat','back-repeat','replaced-repeat','gap-retained','home-interrupted']

def run(command,**kwargs):
    return subprocess.run(list(map(str,command)),check=True,**kwargs)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--runtime',type=Path,required=True)
    p.add_argument('--utilities',type=Path,required=True)
    p.add_argument('--display-sdk',type=Path,required=True)
    p.add_argument('--baseline-inputs',type=Path,required=True)
    p.add_argument('--output-dir',type=Path,default=ROOT/'build/core-paper-motion')
    p.add_argument('--xtensa-cc',type=Path,default=ROOT.parent/'watch-build-tools/platformio-core/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc')
    p.add_argument('--skip-target',action='store_true')
    p.add_argument('--normal-only',action='store_true')
    a=p.parse_args();out=a.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ,NATIVE_APP_CC=str(a.xtensa_cc.resolve()),ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    receipt={'purpose':'actual core app/adapter gesture and native ELF qualification','hardware':'not run','publication':'none','native':{},'host':{}}
    for app in ['settings','file_browser','wifi_settings']:
        product=out/app
        if not a.skip_target:
            opts=['--settings-profile' if app=='settings' else '--time-profile','x4-native-time',
                  '--native-time-runtime-repo',a.runtime,'--tagged-alarm-utilities',a.utilities,
                  '--alarm-client','--quick-actions','--paper-transitions','--stage-logs','--home-app','default.elf',
                  '--return-app','springboard.elf','--output-dir',product]
            if app=='settings':opts+=['--performance-runtime-repo',a.runtime,'--performance-display-sdk',a.display_sdk,'--alarm-settings','--quick-radios']
            elif app=='file_browser':opts+=['--storage-capability','storage.volume','--storage-instance','9','--file-handlers']
            else:opts+=['--wifi-instance','15']
            builder='wifi' if app=='wifi_settings' else app
            command=['python',ROOT/'scripts'/('build_portable_'+builder+'.py'),*opts]
            with (out/(app+'-target.log')).open('w') as log:run(command,env=env,stdout=log,stderr=subprocess.STDOUT)
            receipt['native'][app]={'command':list(map(str,command))}
        manifest=json.loads((product/(app+'.json')).read_text())
        record=json.loads((product/(app+'-build-record.json')).read_text())
        old=json.loads((a.baseline_inputs/app/(app+'-build-record.json')).read_text())
        assert manifest['version']==record['version']==paper.CORE_MOTION_VERSIONS[app]
        if not a.skip_target:assert record['paper_motion']==paper.motion_receipt(ROOT)
        for key in ['required_grants','native_time_sdk','tagged_alarm_sdk','native_time_runtime_commit','native_time_sdk_headers']:
            if key in old:
                expected=old[key]
                # 1.5.8 corrects the previously unacquirable installed-files@9
                # selection to the admitted SD volume; all other grants stay put.
                if app=='file_browser' and key=='required_grants':
                    expected=[dict(g,capability='storage.volume') if g['capability']=='storage.installed-files' and g['instance_id']==9 else g for g in expected]
                assert record[key]==expected,(app,key)
        flags=record.get('build_defines',record.get('defines'));old_flags=old.get('build_defines',old.get('defines'))
        assert '-DPORTABLE_PAPER_TRANSITIONS' in flags and '-DPORTABLE_STAGE_LOGS' in flags
        normalize=lambda fs:[f for f in fs if f!='-DPORTABLE_PAPER_TRANSITIONS' and not f.startswith(('-DPORTABLE_SETTINGS_VERSION=','-DPORTABLE_WIFI_VERSION='))]
        if app=='file_browser':old_flags=[f.replace('PORTABLE_FILE_BROWSER_CAPABILITY="storage.installed-files"','PORTABLE_FILE_BROWSER_CAPABILITY="storage.volume"') for f in old_flags]
        assert normalize(flags)==normalize(old_flags),(app,flags,old_flags)
        assert record['sha256']==hashlib.sha256((product/(app+'.elf')).read_bytes()).hexdigest()
        if app!='settings':
            admission=json.loads((product/'x4-native-app.json').read_text())
            assert admission['paper_motion']==record['paper_motion'] and admission['version']==manifest['version']
        receipt['native'].setdefault(app,{}).update(version=manifest['version'],sha256=record['sha256'],size_bytes=record['size_bytes'],sdk_contracts_unchanged=True,grants_unchanged=app!='file_browser')
        include=product/('performance-sdk/include' if app=='settings' else 'native-time-sdk/include')
        flags=['-DPORTABLE_PAPER_TRANSITIONS','-DPORTABLE_NATIVE_CUSTODY_FENCE','-DALARM_SERVICE_TAGGED_V2']
        if app=='settings':
            flags+=['-DTEST_CORE_SETTINGS','-DTEST_NATIVE_SETTINGS_QUICK']
            entry=ROOT/'Apps/settings_native_entry.c';helpers=[ROOT/s for s in settings.NATIVE_TIME_SOURCES]
        else:
            flags+=['-DPORTABLE_NATIVE_TIME_TOOLBAR','-DTEST_NATIVE_TOOLBAR_QUICK','-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_HOME_APP="default.elf"']
            entry=ROOT/'test/native_apps/native_system_app_entry.c';helpers=[ROOT/s for s in native.SOURCES]
            if app=='file_browser':
                # Run the exact compiled Files configuration. A separately
                # hardcoded host selection previously concealed this defect.
                flags=list(record['defines'])+['-DTEST_NATIVE_TOOLBAR_QUICK']
            else:flags+=['-DPORTABLE_WIFI_SETTINGS_APP','-DPORTABLE_WIFI_INSTANCE=15u','-DPORTABLE_WIFI_STORAGE_INSTANCE=6','-DWIFI_RETURN_APP="springboard.elf"']
        sources=[entry,ROOT/'test/native_apps/core_paper_motion_test.c',*helpers,*[ROOT/'lib/PortableApps/src'/n for n in ['quick_actions.c','quick_render.c','quick_session.c']+(['quick_radios.c'] if app=='settings' else [])]]
        for sanitized in ([False] if a.normal_only else [False,True]):
            label=app+('-asan-ubsan' if sanitized else '-normal');binary=out/label
            san=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
            run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function',*san,*flags,'-I'+str(include),'-I'+str(ROOT/'lib/NativeApps/include'),*sources,'-Wl,--wrap=free','-o',binary])
            results=[]
            with (out/(label+'.log')).open('w') as log:
                for case in CASES+(['backlight-latest'] if app=='settings' else []):
                    frames=out/(label+'-'+case);frames.mkdir(exist_ok=True)
                    r=run([binary,case,frames],env=env,stdout=subprocess.PIPE,text=True,timeout=20)
                    log.write(r.stdout);results.append(json.loads(r.stdout))
            receipt['host'][label]=results
            print(label+': '+str(len(results))+' actual gesture cases passed',flush=True)
    receipt['source_sha256']={name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in ['scripts/test_core_paper_motion.py','test/native_apps/core_paper_motion_test.c','test/native_apps/native_system_apps_test.c','test/native_apps/portable_native_time_settings_test.c']}
    receipt.update(source_commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),working_tree_dirty=bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True).strip()))
    (out/'evidence.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print('Core paper motion evidence: '+str(out/'evidence.json'))

if __name__=='__main__':main()

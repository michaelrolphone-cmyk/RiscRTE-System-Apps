#!/usr/bin/env python3
"""Actual native app controllers plus production adapter under custody faults."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import portable_native_toolbar_build as native
ROOT=Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--sdk-include',type=Path,required=True,help='Generated native toolbar SDK include directory')
    p.add_argument('--output-dir',type=Path,default=ROOT/'build/native-app-custody')
    a=p.parse_args();out=a.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
    flags=['-DPORTABLE_NATIVE_TIME_TOOLBAR','-DPORTABLE_NATIVE_CUSTODY_FENCE','-DALARM_SERVICE_TAGGED_V2',
           '-DTEST_NATIVE_TOOLBAR_QUICK','-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_QUICK_RADIOS','-DPORTABLE_ALARM_CLIENT',
           '-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_HOME_APP="default.elf"','-DTEST_NATIVE_APP_CUSTODY','-DPORTABLE_WIFI_INSTANCE=15u']
    apps={
      'wifi':(['-DPORTABLE_WIFI_SETTINGS_APP','-DPORTABLE_WIFI_STORAGE_INSTANCE=6'],['credentials','wifi']),
      'files':(['-DPORTABLE_FILE_BROWSER_APP','-DPORTABLE_NOVA_UI','-DPORTABLE_APP_OWNS_TOUCH_CHROME',
                '-DPORTABLE_FILE_BROWSER_CAPABILITY="storage.volume"','-DPORTABLE_FILE_BROWSER_INSTANCE=9u','-DPORTABLE_FILE_BROWSER_HANDLERS'],['volume','handlers']),
      'firmware':(['-DPORTABLE_UPDATE_APP','-DPORTABLE_UPDATE_FIRMWARE=1'],['update','credentials','wifi']),
      'apps':(['-DPORTABLE_UPDATE_APP','-DPORTABLE_UPDATE_FIRMWARE=0'],['update','credentials','wifi'])}
    records={}
    with tempfile.TemporaryDirectory(prefix='native-app-custody-') as t:
      stage=Path(t);inc=stage/'include'
      shutil.copytree(ROOT/'lib/PortableApps/include',inc);shutil.copytree(ROOT/'lib/PortableApps/time',stage/'time')
      for name in (*native.SDK_HEADERS,*native.portable_alarm_build.HEADERS):shutil.copyfile(a.sdk_include/name,inc/name)
      for app,(extra,targets) in apps.items():
       cases=[('normal','none'),('reopen','none')]
       for target in targets:
        cases += [(fault,target) for fault in ['acquire-empty','acquire-dirty','grant-generation','grant-slot','grant-size','grant-api']]
       cases += [(fault,'none') for fault in ['release-false','release-dirty','release-size']]
       if app=='files':cases += [(fault,'none') for fault in ['file-close','dir-close','read-error','write-error','handoff-release']]
       else:cases += [(fault,'none') for fault in ['kv-context','kv-context-2','kv-context-4','kv-unknown','kv-unknown-3','kv-io','connect-error','disconnect']]
       if app=='wifi':cases += [(fault,'none') for fault in ['put-context','put-context-3','put-unknown','put-unknown-2','scan-cancel']]
       if app in ('firmware','apps'):cases += [('cancel','none')]
       for broadcast in (False,True):
        for sanitized in (False,True):
         label=app+('-broadcast' if broadcast else '')+('-sanitized' if sanitized else '')
         binary=out/label
         variants=(['-DPORTABLE_BLE_BROADCAST','-DPORTABLE_BLE_BROADCAST_DEFAULT_OFF','-DPORTABLE_PAPER_PREFERENCES'] if broadcast else [])
         san=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
         command=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-misleading-indentation',*flags,*extra,*variants,*san,
           '-I'+str(inc),'-I'+str(ROOT/'lib/NativeApps/include'),ROOT/'test/native_apps/native_app_custody_test.c',ROOT/'test/native_apps/native_system_app_entry.c',
           *[ROOT/s for s in native.SOURCES],*[ROOT/'lib/PortableApps/src'/s for s in ['quick_actions.c','quick_render.c','quick_session.c','quick_radios.c']],'-Wl,--wrap=free','-o',binary]
         subprocess.run(list(map(str,command)),check=True)
         with (out/(label+'.log')).open('w') as log:
          for fault,target in cases+([('telemetry-recursion','none')] if broadcast else []):
           result=subprocess.run([binary,fault,target],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,
             env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'),timeout=20)
           log.write(result.stdout);log.flush()
           if result.returncode:raise RuntimeError(f'{label} {fault} {target}: {result.stdout}')
         records[label]=cases+([('telemetry-recursion','none')] if broadcast else []);print(label+': '+str(len(records[label]))+' cases passed',flush=True)
    (out/'evidence.json').write_text(json.dumps({'cases':records,'hardware':'not run','publication':'none'},indent=2)+'\n')
if __name__=='__main__':main()

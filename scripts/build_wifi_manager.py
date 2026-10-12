#!/usr/bin/env python3
"""Build the native scene-only Wi-Fi Manager and its declared provider contracts."""
import argparse, json, shutil, subprocess, tempfile
from pathlib import Path
from scene_sdk import stage_sdk
from scene_build import build, compiler_path
ROOT=Path(__file__).resolve().parents[1]

def sdk(runtime, output):
    inc=stage_sdk(runtime,ROOT,output)
    for name in ('RiscTextEntryV1.h',):shutil.copyfile(ROOT/'sdk/app'/name,inc/name)
    # The deployed net.wifi contract is owned by the portable provider SDK.
    for name in ('WifiApi.h','RiscRadioAsyncV1.h','RiscRadioScanV1.h'):
        source=ROOT/'lib/PortableApps/include'/name
        dest=inc/name
        # Runtime owns native suffixes; WifiApi consumes only their shared prefix.
        if not dest.exists():shutil.copyfile(source,dest)
    for source in (ROOT/'lib/PortableApps/include').glob('*.h'):
        if not (inc/source.name).exists():shutil.copyfile(source,inc/source.name)
    return inc

def run(runtime, output):
    if output.exists():raise FileExistsError(output)
    with tempfile.TemporaryDirectory(prefix='wifi-manager-sdk-') as tmp:
        inc=sdk(runtime,Path(tmp))
        m=json.loads((ROOT/'Apps/native/wifi_settings.json').read_text())
        row=build(compiler_path(),inc,output,m,[ROOT/'Apps/wifi_manager/app.c'],
                  ['-Wno-misleading-indentation','-Wframe-larger-than=1536'],
                  exports={'app_main','risc_resident_app_descriptor_v1'})
        shutil.copyfile(ROOT/'Apps/native/wifi_settings.json',output/'manifest.json')
    return row
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();print(json.dumps(run(a.runtime.resolve(),a.output.resolve()),indent=2))

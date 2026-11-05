#!/usr/bin/env python3
"""Build the native scene-only Wi-Fi Manager and its declared provider contracts."""
import argparse, hashlib, json, shutil, subprocess, tempfile
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
    # App-owned bridge must not be shadowed by an older staged SDK header.
    shutil.copyfile(ROOT/'lib/PortableApps/include/PortableForegroundBroadcast.h',inc/'PortableForegroundBroadcast.h')
    for source in (ROOT/'lib/RemoteFiles/vendor/tinydtls_sha2').glob('*.h'):
        shutil.copyfile(source,inc/source.name)
    return inc

def run(runtime, output):
    if output.exists():raise FileExistsError(output)
    with tempfile.TemporaryDirectory(prefix='wifi-manager-sdk-') as tmp:
        inc=sdk(runtime,Path(tmp))
        m=json.loads((ROOT/'Apps/wifi_manager/manifest.json').read_text())
        row=build(compiler_path(),inc,output,m,[ROOT/'Apps/wifi_manager/app.c',ROOT/'lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c'],
                  ['-Wno-misleading-indentation','-Wframe-larger-than=1536'],
                  exports={'app_main','risc_resident_app_descriptor_v1'})
        shutil.copyfile(ROOT/'Apps/wifi_manager/manifest.json',output/'manifest.json')
    row['source_pins']={
        'runtime':subprocess.check_output(['git','-C',str(runtime),'rev-parse','HEAD'],text=True).strip(),
        'system':subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip()}
    row['source_commit']=row['source_pins']['system']
    row['manifest_sha256']=hashlib.sha256((output/'manifest.json').read_bytes()).hexdigest()
    row['working_tree_dirty']=subprocess.run(['git','-C',str(ROOT),'diff','--quiet','HEAD','--']).returncode!=0
    (output/'build.json').write_text(json.dumps(row,indent=2,sort_keys=True)+'\n')
    return row
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();print(json.dumps(run(a.runtime.resolve(),a.output.resolve()),indent=2))

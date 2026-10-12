#!/usr/bin/env python3
"""Run actual app, shared scene and keyboard against bounded radio/storage fixtures."""
import argparse, os, subprocess
from pathlib import Path
from build_wifi_manager import sdk
ROOT=Path(__file__).resolve().parents[1]
MODES=('behavior','busy-storage','storage-failure','old-scene','missing-keyboard','unsupported','cancel','loss','failure','forget','hidden-network','keyboard-cancel','storage-loss','bad-progress')
def run(runtime, output, sanitize=False):
    # The app must stay semantic: no private renderer, keyboard or input driver.
    text='\n'.join(p.read_text() for p in (ROOT/'Apps/wifi_manager').glob('*') if p.suffix in ('.c','.inc','.h'))
    for forbidden in ('RiscDisplayV1.h','PortableKeyboard','input.touch.raw','display.output','risc_display_v1','draw_pixel','nc_draw','kp_render'):
        assert forbidden not in text, 'App duplicated shared UI: '+forbidden
    assert 'RISC_TEXT_ENTRY_REQUEST_MASKED' in text and 'risc_text_entry_masked' in text
    output.mkdir(parents=True,exist_ok=True);inc=sdk(runtime,output/'sdk')
    for name in ('RiscUsbHidV1.h','RiscUsbControllerV1.h','RiscUsbProviderV1.h'):
        (inc/name).write_bytes((ROOT/'Services/text_input'/name).read_bytes())
    flags=['-std=c11','-g','-O1','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-I'+str(inc)]
    if sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
    cc=os.environ.get('CC','cc');obj=output/'text.o'
    subprocess.run([cc,*flags,'-Dt5_driver_get=text_driver_get','-c',str(ROOT/'Services/text_input/host.c'),'-o',str(obj)],check=True)
    exe=output/'test'
    subprocess.run([cc,*flags,str(ROOT/'Services/scene_host/host.c'),str(ROOT/'test/wifi_manager/app_test.c'),str(obj),'-o',str(exe)],check=True)
    for mode in MODES:
        d=output/'frames'/mode;d.mkdir(parents=True,exist_ok=True)
        subprocess.run([str(exe),mode,str(d)],check=True,timeout=30,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
    print(f'{len(MODES)} actual Wi-Fi app/scene/text service scenarios PASS; physical hardware not tested')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--runtime',required=True,type=Path);p.add_argument('--output',required=True,type=Path);p.add_argument('--sanitize',action='store_true');a=p.parse_args();run(a.runtime.resolve(),a.output.resolve(),a.sanitize)

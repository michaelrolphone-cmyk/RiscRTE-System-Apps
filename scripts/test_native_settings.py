#!/usr/bin/env python3
"""Capability-sized Settings against the production controller and raw providers.

The retained fixture is staged with dimensioned provider storage only. Native
interaction expectations are independent and never call the legacy coordinate
translation. All generated files remain under the requested output directory.
"""
import argparse, hashlib, json, os, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SIZES=((240,240),(410,502),(128,160),(160,128),(502,240))
def stage_fixture(out, name):
    src=(ROOT/'test/native_apps'/name).read_text()
    src=src.replace('#include "../../lib/PortableApps/src/adapter.c"',f'#include "{ROOT}/lib/PortableApps/src/adapter.c"')
    src=src.replace('#include "nova_settings_coordinates.h"','')
    src=src.replace('static uint16_t framebuffer[240*240];','static uint16_t *framebuffer;')
    src=src.replace('static uint16_t framebuffer[240 * 240];','static uint16_t *framebuffer;')
    src=src.replace('.width=240,.height=240','.width=test_width,.height=test_height')
    src=src.replace('.stride_bytes=480,.size_bytes=sizeof(framebuffer)', '.stride_bytes=test_stride,.size_bytes=test_size')
    src=src.replace('out->width=out->height=240;', 'out->width=test_width;out->height=test_height;')
    src=src.replace('    nova_fixture_choice_coordinates(sv_page,&out->contacts[0].x,&out->contacts[0].y);','')
    src=src.replace('239-out->contacts[0].x','test_width-1-out->contacts[0].x').replace('239-out->contacts[0].y','test_height-1-out->contacts[0].y')
    dst=out/name;dst.write_text(src);return dst

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=ROOT/'build/native-settings')
    args=parser.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    fixture=stage_fixture(out,'portable_alarm_settings_test.c');runs=[]
    env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'}
    for san in (False,True):
        for rotation in (0,180):
            name=f'native-settings-{rotation}-{int(san)}';binary=out/name;frames=out/(name+'-frames');frames.mkdir(exist_ok=True)
            cmd=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function',
                 '-DPORTABLE_SETTINGS_NATIVE_UI','-DPORTABLE_NATIVE_CANVAS','-DPORTABLE_NOVA_UI','-DPORTABLE_SLEEP_SETTINGS',
                 '-DPORTABLE_TOUCH_ROTATION='+str(rotation),f'-DSETTINGS_FRAME_FIXTURE="{fixture}"',
                 '-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include')]
            if san:cmd+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
            cmd += [str(ROOT/'Apps/settings.c'),str(ROOT/'test/native_apps/native_settings_test.c'),'-o',str(binary)]
            subprocess.run(cmd,check=True,timeout=120)
            for w,h in SIZES:
                result=subprocess.run([str(binary),str(frames),str(w),str(h)],env=env,check=True,capture_output=True,text=True,timeout=30)
                print(result.stdout,end='');runs.append({'size':[w,h],'rotation':rotation,'sanitizers':san,'result':'passed'})
            binary.unlink()
    hashes={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [ROOT/'test/native_apps/native_settings_test.c',*sorted((ROOT/'lib/PortableApps/src').glob('settings*.inc'))]}
    (out/'receipt.json').write_text(json.dumps({'source_base':'fd9a028c5d38eb517c1328b869684c4330a5d4d9','runs':runs,'source_sha256':hashes,'hardware':'not-tested'},indent=2)+'\n')
    print(f'Native Settings: {len(runs)} actual-controller runs passed')
if __name__=='__main__':main()

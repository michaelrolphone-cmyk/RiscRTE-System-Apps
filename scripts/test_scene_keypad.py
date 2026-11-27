#!/usr/bin/env python3
"""Exercise reusable keypad against the real presenter and capability geometry."""
import argparse, os, subprocess, tempfile
from pathlib import Path
from scene_sdk import stage_sdk
ROOT = Path(__file__).resolve().parents[1]
def run(runtime, output, sanitize=False):
    include = stage_sdk(runtime, ROOT, output / 'sdk')
    flags=['-std=c11','-O1','-Wall','-Wextra','-Werror']
    if sanitize: flags += ['-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
    env=dict(os.environ);env.setdefault('ASAN_OPTIONS','detect_leaks=0')
    for profile,profile_flags in [('full',[]),('color-dynamic',['-DSCENE_HOST_TELEMETRY=0'])]:
        executable=output/('keypad-test-'+profile)
        subprocess.run([os.environ.get('CC','cc'),*flags,*profile_flags,'-I'+str(include),str(ROOT/'Services/scene_host/host.c'),str(ROOT/'test/keypad/provider_test.c'),'-o',str(executable)],check=True)
        for shape in range(6):
            env['KEYPAD_PPM']=str(output/f'keypad-{profile}-{shape}.ppm')
            subprocess.run([str(executable),str(shape)],env=env,check=True)
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--sanitize',action='store_true');p.add_argument('--output',type=Path)
    a=p.parse_args()
    if a.output:a.output.mkdir(parents=True,exist_ok=True);run(a.runtime.resolve(),a.output.resolve(),a.sanitize)
    else:
        with tempfile.TemporaryDirectory(prefix='scene-keypad-') as t:run(a.runtime.resolve(),Path(t),a.sanitize)

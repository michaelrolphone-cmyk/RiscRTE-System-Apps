#!/usr/bin/env python3
"""Build one optional presenter and independently selected class-profile ELFs."""
import argparse
import json
import os
import subprocess
from pathlib import Path
import tempfile
from scene_build import build, compiler_path, json_write
from scene_sdk import stage_sdk
ROOT=Path(__file__).resolve().parents[1]


def run(runtime: Path, output: Path) -> None:
    if output.exists():
        raise FileExistsError(f'Use a new output directory: {output}')
    output.mkdir(parents=True)
    compiler=compiler_path()
    with tempfile.TemporaryDirectory(prefix='scene-sdk-') as temporary:
        include=stage_sdk(runtime,ROOT,Path(temporary))
        host=json.loads((ROOT/'Services/scene_host/manifest.json').read_text())
        rows=[build(compiler,include,output/'scene-host',host,[ROOT/'Services/scene_host/host.c'])]
        for name,flags in (('compact-color',[]),('portrait-monochrome',
                           ['-DSCENE_PROFILE_PAPER=1','-DSCENE_DISPLAY_ROTATION=90'])):
            identity='scene-profile-'+name
            profile=json.loads((ROOT/'Services/scene_profile'/(name+'.json')).read_text())
            if profile['id']!=identity:raise ValueError('Presentation package identity mismatch')
            flags=[*flags,f'-DSCENE_PROFILE_ID="{identity}"']
            row=build(compiler,include,output/name,profile,[ROOT/'Services/scene_profile/profile.c'],flags)
            row['profile']=name;rows.append(row)
        validator=Path(temporary)/'validate-elf'
        subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',
                        '-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),
                        str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'),
                        str(ROOT/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
        for elf in output.rglob('*.elf'):subprocess.run([str(validator),str(elf)],check=True)
    json_write(output/'packages.json',{'schema':1,'packages':rows,'optional':True})
    print('Built optional scene host and two external presentation profiles.')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();run(a.runtime.resolve(),a.output.resolve())

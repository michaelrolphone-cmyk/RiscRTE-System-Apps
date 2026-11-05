#!/usr/bin/env python3
"""Build one optional presenter and independently selected class-profile ELFs."""
import argparse
import hashlib
import json
import os
import subprocess
import shutil
from pathlib import Path
import tempfile
from scene_build import build, compiler_path, json_write
from scene_sdk import stage_sdk
ROOT=Path(__file__).resolve().parents[1]
# Scene rotations use logical-to-surface coordinates. Installed X4 adapter90
# corresponds to scene270: (x,y) -> (y,479-x). GT911 is already portrait.
HOST_PROFILE_FLAGS = {
    'full': [],
    'watch-compact': ['-DSCENE_HOST_TELEMETRY=0','-DSCENE_HOST_WATCH_240=1'],
    'color-dynamic': ['-DSCENE_HOST_TELEMETRY=0'],
}

def host_manifest(profile):
    if profile not in HOST_PROFILE_FLAGS:raise ValueError('Unknown scene host profile: '+profile)
    name='manifest-watch-compact.json' if profile in ('watch-compact','color-dynamic') else 'manifest.json'
    host=json.loads((ROOT/'Services/scene_host'/name).read_text())
    return host

PROFILE_FLAGS = {
    'compact-color': [],
    'portrait-monochrome': ['-DSCENE_PROFILE_PAPER=1', '-DSCENE_DISPLAY_ROTATION=270'],
}


def scene_memory_receipt(compiler, include, temporary, host_profile):
    """Measure target ABI layouts in a throwaway object, never the shipped ELF."""
    source=temporary/'scene-memory.c';obj=temporary/'scene-memory.o'
    source.write_text('#include '+json.dumps(str(ROOT/'Services/scene_host/host.c'))+'\n'
                      'const unsigned char scene_model_size_probe[sizeof(scene_model)]={0};\n'
                      'const unsigned char modal_checkpoint_size_probe[sizeof(modal_checkpoint)]={0};\n')
    subprocess.run([compiler,'-std=c11','-Os','-I'+str(include),*HOST_PROFILE_FLAGS[host_profile],
                    '-c',str(source),'-o',str(obj)],check=True)
    symbols=subprocess.check_output([compiler.removesuffix('gcc')+'nm','-S','--defined-only',str(obj)],text=True)
    sizes={}
    for line in symbols.splitlines():
        fields=line.split()
        if len(fields)==4 and fields[3] in ('scene_model_size_probe','modal_checkpoint_size_probe'):
            sizes[fields[3]]=int(fields[1],16)
    if len(sizes)!=2:raise ValueError('Missing target scene memory-size witnesses')
    return {'target_scene_model_bytes':sizes['scene_model_size_probe'],
            'modal_additional_heap_bytes':sizes['modal_checkpoint_size_probe'],
            'modal_max_depth':1,'duplicates_parent_pixel_buffers':False,
            'allocator_overhead_included':False,'heap_admission':'not established by size witness'}


def run(runtime: Path, output: Path, host_profile: str = 'full') -> None:
    source_pins={name:subprocess.check_output(['git','-C',str(path),'rev-parse','HEAD'],text=True).strip()
                 for name,path in [('runtime',runtime),('system',ROOT)]}
    # Build outputs and separately checked-out dependencies are not tracked source.
    working_tree_dirty=any(subprocess.check_output(['git','-C',str(path),'status','--porcelain','--untracked-files=no'],text=True).strip()
                           for path in (runtime,ROOT))
    if output.exists():
        raise FileExistsError(f'Use a new output directory: {output}')
    output.mkdir(parents=True)
    compiler=compiler_path()
    with tempfile.TemporaryDirectory(prefix='scene-sdk-') as temporary:
        include=stage_sdk(runtime,ROOT,Path(temporary))
        host=host_manifest(host_profile)
        rows=[build(compiler,include,output/'scene-host',host,[ROOT/'Services/scene_host/host.c'],HOST_PROFILE_FLAGS[host_profile])]
        rows[0]['host_profile']=host_profile
        rows[0]['memory']=scene_memory_receipt(compiler,include,Path(temporary),host_profile)
        rows[0]['optional_apis']={'telemetry':host_profile=='full','components':True,'page':True,'checkpoint':True,'reading':True,'reading_links':True,'connectivity':True,'modal':True}
        json_write(output/'scene-host/build.json',rows[0])
        license_dir=output/'scene-host/licenses';license_dir.mkdir()
        for name in ['LICENSE-Orbitron.txt','LICENSE-Rajdhani.txt','SOURCES.json','COMPONENT-SOURCES.json']:
            shutil.copyfile(ROOT/'Services/scene_host/fonts'/name,license_dir/name)
        for name,flags in PROFILE_FLAGS.items():
            identity='scene-profile-'+name
            profile=json.loads((ROOT/'Services/scene_profile'/(name+'.json')).read_text())
            if profile['id']!=identity:raise ValueError('Presentation package identity mismatch')
            flags=[*flags,f'-DSCENE_PROFILE_ID="{identity}"']
            row=build(compiler,include,output/name,profile,[ROOT/'Services/scene_profile/profile.c'],flags)
            row['profile']=name;rows.append(row)
        for row,package in zip(rows,[output/'scene-host',*(output/name for name in PROFILE_FLAGS)]):
            row.update(source_pins=source_pins,source_commit=source_pins['system'],working_tree_dirty=working_tree_dirty,
                       manifest_sha256=hashlib.sha256((package/'manifest.json').read_bytes()).hexdigest())
            json_write(package/'build.json',row)
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
    p.add_argument('--host-profile',choices=HOST_PROFILE_FLAGS,default='full')
    a=p.parse_args();run(a.runtime.resolve(),a.output.resolve(),a.host_profile)

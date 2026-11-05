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
from scene_build import build, compiler_path, json_write, target_flags
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


def scene_stack_receipt(compiler, include, temporary, host_profile, package, source_pins, working_tree_dirty, build_receipt):
    """Measure the original translation unit and require an identical target ELF."""
    source=ROOT/'Services/scene_host/host.c';obj=temporary/'scene-stack.o'
    mapping=temporary/'scene-stack.map'
    mapping.write_text('{ global: t5_driver_get; local: *; };\n')
    flags=target_flags(mapping)
    subprocess.run([compiler,*flags,'-fstack-usage','-I'+str(include),
                    *HOST_PROFILE_FLAGS[host_profile],'-c',str(source),'-o',str(obj)],check=True)
    witness=temporary/'scene-stack.elf'
    subprocess.run([compiler,*flags,str(obj),'-o',str(witness)],check=True)
    if witness.read_bytes()!=(package/'driver.elf').read_bytes():
        raise ValueError('Stack witness ELF differs from the shipped scene provider')
    files=sorted(temporary.glob('scene-stack*.su'))
    if not files:raise ValueError('Target compiler did not emit stack-usage records')
    records=[]
    for path in files:
        for line in path.read_text().splitlines():
            location,size,qualifiers=line.rsplit('\t',2)
            source_location,function=location.rsplit(':',1)
            records.append({'function':function,'bytes':int(size),
                            'qualifiers':qualifiers,'source':source_location.replace(str(ROOT)+'/','')})
    records.sort(key=lambda row:(row['function'],row['source']))
    required=('modal_open','modal_close','modal_restore','scene_open','components_open',
              'open_document','scene_next','scene_close','scene_pause','scene_resume',
              'close_owned','detach_owned','t5_driver_get')
    selected={name:[row for row in records if row['function']==name or row['function'].startswith(name+'.')]
              for name in required}
    if not selected['modal_open'] or not selected['t5_driver_get']:
        raise ValueError('Missing required address-taken provider stack records')
    raw='\n'.join(path.read_text().rstrip() for path in files)+'\n'
    (package/'stack-usage.su').write_text(raw.replace(str(ROOT)+'/',''))
    receipt={'schema':1,'compiler':subprocess.check_output([compiler,'--version'],text=True).splitlines()[0],
             'compile_flags':[flag.replace(str(mapping),'<export-map>') for flag in flags]+['-fstack-usage'],
             'defines':HOST_PROFILE_FLAGS[host_profile],'elf_byte_identical':True,
             'elf_sha256':hashlib.sha256(witness.read_bytes()).hexdigest(),
             'raw_sha256':hashlib.sha256((package/'stack-usage.su').read_bytes()).hexdigest(),
             'records':records,'selected':selected,'source_pins':source_pins,'working_tree_dirty':working_tree_dirty,
             'source_sha256':build_receipt['source_sha256'],'sdk_sha256':build_receipt['sdk_sha256'],
             'limits':['Per-function compiler frames, not cumulative call-chain or interrupt stack demand.',
                       'Empty selected lists mean inlined or no separately emitted function, never zero stack.',
                       'Dynamic qualifiers are preserved; no guessed bound or product stack admission.']}
    json_write(package/'stack-usage.json',receipt)
    summary={'elf_byte_identical':True,'receipt':'stack-usage.json','raw':'stack-usage.su','selected':selected,
             'product_stack_admission':'not established'}
    print('Scene target stack '+json.dumps({'profile':host_profile,**summary},sort_keys=True))
    return summary


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
        rows[0]['stack']=scene_stack_receipt(compiler,include,Path(temporary),host_profile,output/'scene-host',source_pins,working_tree_dirty,rows[0])
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

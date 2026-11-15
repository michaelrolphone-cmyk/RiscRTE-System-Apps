#!/usr/bin/env python3
"""Build one canonical semantic Firmware Update app; product routing is external."""
import argparse, hashlib, json, os, re, shlex, shutil, subprocess, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('runtime', 'utilities', 'compiler', 'legacy-linker', 'output'):
        p.add_argument('--'+name, type=Path, required=True)
    a = p.parse_args()
    runtime, compiler, linker, out = (x.resolve() for x in (a.runtime,a.compiler,a.legacy_linker,a.output))
    out.mkdir(parents=True, exist_ok=False)
    version = subprocess.check_output([str(compiler),'--version'],text=True)
    if 'esp-15.2.0_20251204' not in version: raise ValueError('Pinned SDK 6.1 compiler required')
    utilities=a.utilities.resolve()
    source = ROOT/'Apps/ota_update_scene.c'
    sources=[source,ROOT/'Apps/ota_update_scene_background.c']
    manifest_path = ROOT/'Apps/ota_update_scene.json'
    manifest = json.loads(manifest_path.read_text())
    if manifest.get('requires') or manifest['file_name'] != 'ota_update.elf':
        raise ValueError('Optional canonical application manifest required')
    declarations = manifest.get('optional',[])
    if len(declarations)>32 or len({(x['capability'],x['api']) for x in declarations})!=len(declarations):
        raise ValueError('Invalid optional capability declaration set')
    # Stage exactly one transitive header identity, preferring the selected Runtime.
    sdk=out/'sdk';sdk.mkdir(); records={}
    roots=[runtime/'sdk/app', ROOT/'sdk/app', ROOT/'sdk/driver',ROOT/'lib/PortableApps/include',ROOT/'lib/NativeApps/include',ROOT/'Apps',runtime/'sdk/driver',runtime/'sdk/hardware',utilities/'Apps',utilities/'lib/Contexts/include',utilities/'lib/Alarm/include',utilities/'lib/Bluetooth/include']
    include=re.compile(r'^\s*#\s*include\s*"([^"]+)"',re.M)
    pending=[name for path in sources for name in include.findall(path.read_text())]; seen=set()
    while pending:
        name=pending.pop()
        if name in seen: continue
        if Path(name).name!=name: raise ValueError('Noncanonical include path '+name)
        original=next((root/name for root in roots if (root/name).is_file()),None)
        if original is None: raise ValueError('Missing selected header '+name)
        seen.add(name);shutil.copyfile(original,sdk/name)
        records[name]={'source':str(original),'sha256':sha(original)}
        pending.extend(include.findall(original.read_text()))
    bridge=out/'linker';bridge.mkdir()
    shim=bridge/'ld'
    shim.write_text('#!/usr/bin/env python3\nimport os,sys\na=sys.argv[1:]\nassert not any(x.startswith("--dynconfig=") and x!="--dynconfig=xtensa_esp32s3.so" for x in a)\nos.execv('+repr(str(linker))+',['+repr(str(linker))+']+[x for x in a if x!="--dynconfig=xtensa_esp32s3.so"])\n')
    shim.chmod(0o755);(bridge/(compiler.name.removesuffix('gcc')+'ld')).symlink_to(shim)
    expected_exports={'app_main','risc_resident_app_descriptor_v1'}
    exports=out/'exports.map';exports.write_text('{ global: '+ '; '.join(sorted(expected_exports))+'; local: *; };\n')
    commands=[]; builds=[]
    common=[str(compiler),'-B'+str(bridge)+'/', '-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wno-unused-function','-I'+str(sdk)]
    for repeat in ('canonical','repeat'):
        directory=out/repeat;directory.mkdir();objects=[];dependencies={};frames={}
        for index,unit in enumerate(sources):
            obj=directory/('unit-'+str(index)+'.o');dep=obj.with_suffix('.d');objects.append(obj)
            command=[*common,'-fstack-usage','-MMD','-MF',str(dep),'-c',str(unit),'-o',str(obj)]
            commands.append(command);subprocess.run(command,check=True)
            for line in obj.with_suffix('.su').read_text().splitlines():
                function,size,kind=line.split('\t');frames[function]=int(size)
                if kind!='static' or int(size)>2048: raise ValueError('Unbounded or oversized stack frame: '+line)
            paths=[Path(x) for x in shlex.split(dep.read_text().replace('\\\n',' ').split(':',1)[1])]
            dependencies.update({str(x):sha(x) for x in paths})
        elf=directory/'ota_update.elf'
        command=[*common,'-nostdlib','-nostartfiles','-shared','-Wl,--hash-style=sysv','-Wl,--no-relax','-Wl,--version-script='+str(exports),*map(str,objects),'-o',str(elf)]
        commands.append(command);subprocess.run(command,check=True)
        from normalize_xtensa_relocations import normalize
        from compact_current_elf import compact
        normalize(elf);compaction=compact(elf,str(compiler),debug_path=directory/'ota_update.debug.elf')
        symbols=subprocess.check_output([str(compiler).removesuffix('gcc')+'nm','-D',str(elf)],text=True)
        imported={line.split()[-1] for line in symbols.splitlines() if ' U ' in ' '+line}
        exported={line.split()[-1] for line in symbols.splitlines() if len(line.split())>=3 and line.split()[-2] in ('T','D','B','R')}
        allowed={'risc_runtime_get_api','memcpy','memset','memcmp','memmove','memchr','strcmp','strncmp','strlen','snprintf'}
        if exported!=expected_exports or not imported<=allowed: raise ValueError((exported,imported-allowed))
        builds.append({'sha256':sha(elf),'size':elf.stat().st_size,'imports':sorted(imported),'exports':sorted(exported),'stack_frames':frames,'dependencies':dependencies,'compaction':compaction})
    if builds[0]['sha256']!=builds[1]['sha256']: raise ValueError('Repeated canonical build differs')
    validator=out/'validate-elf'
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'),str(ROOT/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
    subprocess.run([str(validator),str(out/'canonical/ota_update.elf')],check=True)
    selections={}
    authority=json.loads((ROOT/'docs/shared-firmware/baseline.json').read_text())['authority']
    for product,lane in (('x4','x4'),('twatch','watch'),('waveshare','wave')):
        destination=out/'selections'/product;destination.mkdir(parents=True)
        shutil.copyfile(out/'canonical/ota_update.elf',destination/'ota_update.elf')
        selected=json.loads(json.dumps(manifest))
        grants=authority[lane]['applications'][0]['row']['grants']
        for declaration in selected['optional']:
            apis={g['api'] for g in grants if g['capability']==declaration['capability']}
            if len(apis)>1:raise ValueError('Ambiguous original declaration API')
            if apis:declaration['api']=next(iter(apis))
        (destination/'ota_update.json').write_text(json.dumps(selected,indent=2)+'\n')
        selections[product]={'sha256':sha(destination/'ota_update.elf'),'manifest_sha256':sha(destination/'ota_update.json')}
    for record in builds:
        for path,digest in record['dependencies'].items():
            if sha(path)!=digest:raise ValueError('Source changed during build: '+path)
    receipt={'schema':1,'builds':builds,'selections':selections,'commands':commands,'headers':records,'compiler':version,'compiler_sha256':sha(compiler),'linker_sha256':sha(linker),'source_sha256':{str(x.relative_to(ROOT)):sha(x) for x in sources},'manifest_sha256':sha(manifest_path),'script_sha256':sha(__file__),'repository_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'utilities_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=utilities,text=True).strip(),'utilities_dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=utilities,text=True).strip()),'runtime_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=runtime,text=True).strip(),'working_tree_dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True).strip()),'product_admission':False,'hardware_tested':False}
    (out/'build.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print('Canonical shared Firmware Update repeated target build and structural validation passed',builds[0]['sha256'])
if __name__=='__main__': main()

#!/usr/bin/env python3
"""Freestanding Xtensa paper projection must need no compiler helper imports."""
import argparse, hashlib, json, os, shlex, subprocess
from pathlib import Path
from native_toolchain import compiler_path

ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--compiler')
p.add_argument('--output',type=Path,default=ROOT/'build/paper-geometry-target')
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
cc=compiler_path(a.compiler);elf=out/'projection.elf';validator=out/'validate-elf'
sources=['lib/PortableApps/include/PortablePaperGeometry.h',
 'test/native_apps/paper_geometry_target.c','scripts/test_paper_geometry_target.py',
 'scripts/native_toolchain.py','lib/elf_loader/src/esp_elf_validate.c',
 'test/native_apps/validate_test.c']
command=[cc,'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls',
 '-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared',
 '-Wl,--hash-style=sysv','-Wall','-Wextra','-Werror',
 '-I'+str(ROOT/'lib/PortableApps/include'),
 str(ROOT/'test/native_apps/paper_geometry_target.c'),'-o',str(elf)]
subprocess.run(command,check=True)
symbols=subprocess.check_output([cc.removesuffix('gcc')+'nm','-D',str(elf)],text=True)
(out/'symbols.txt').write_text(symbols)
imports=[line.split()[-1] for line in symbols.splitlines() if ' U ' in ' '+line]
assert not imports, 'Unexpected freestanding projection imports: '+str(imports)
validate=[os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',
 '-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),
 str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'),
 str(ROOT/'test/native_apps/validate_test.c'),'-o',str(validator)]
subprocess.run(validate,check=True)
result=subprocess.run([str(validator),str(elf)],check=True,capture_output=True,text=True)
closures=[]
for build in [command,validate]:
    dep=subprocess.check_output(build[:-2]+['-MM'],text=True)
    files={Path(x).resolve() for x in shlex.split(dep.replace('\\\n',' '))
           if not x.endswith(':')}
    assert all(x.is_file() for x in files), 'Missing compiler dependency'
    closures.append({'command':build[:-2]+['-MM'],'output':dep,
                     'sha256':{str(x):hashlib.sha256(x.read_bytes()).hexdigest()
                               for x in sorted(files)}})
receipt={'compiler':subprocess.check_output([cc,'--version'],text=True).splitlines()[0],
 'command':command,'validator_command':validate,'imports':imports,'symbols':symbols,
 'compiler_sha256':hashlib.sha256(Path(cc).read_bytes()).hexdigest(),
 'compiler_non_system_dependencies':closures,
 'validator_stdout':result.stdout,'source_sha256':{
  s:hashlib.sha256((ROOT/s).read_bytes()).hexdigest() for s in sources},
 'executable_sha256':{x.name:hashlib.sha256(x.read_bytes()).hexdigest() for x in [elf,validator]}}
(out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
elf.unlink();validator.unlink()
print('Freestanding Xtensa projection: no imports; unchanged ELF admission PASS')

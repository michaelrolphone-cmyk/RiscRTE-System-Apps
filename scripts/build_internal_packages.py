#!/usr/bin/env python3
"""Build the explicit native-volume projection; does not activate firmware."""
from native_toolchain import compiler_path as resolve_compiler
import argparse, hashlib, json, os, shutil, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--runtime-repo',type=Path,required=True)
p.add_argument('--output-dir',type=Path,required=True)
a=p.parse_args();out=a.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
cc = resolve_compiler()
src=root/'Services/internal_packages/service.c';elf=out/'driver.elf'
subprocess.run([cc,'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-Wall','-Wextra','-Werror','-nostdlib','-nostartfiles','-shared','-Wl,--hash-style=sysv',*['-I'+str(a.runtime_repo.resolve()/'sdk'/part) for part in ('app','driver','hardware')],str(src),'-lgcc','-o',str(elf)],check=True)
imports=sorted(line.split()[-1] for line in subprocess.check_output([cc.removesuffix('gcc')+'nm','-D',str(elf)],text=True).splitlines() if ' U ' in ' '+line)
if set(imports)-{'strcmp','memcpy','memset'}:raise ValueError('Unexpected imports: '+repr(imports))
shutil.copyfile(root/'Services/internal_packages/manifest.json',out/'manifest.json')
receipt={'status':'target-link-only','sha256':hashlib.sha256(elf.read_bytes()).hexdigest(),'bytes':elf.stat().st_size,'imports':imports,'source_sha256':hashlib.sha256(src.read_bytes()).hexdigest(),'hardware_tested':False}
(out/'build.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt))

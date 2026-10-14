#!/usr/bin/env python3
"""Rebuild the deployed full-feature Springboard recipe with the T030 sources."""
import argparse,hashlib,json,subprocess,sys
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
for n in ('recipe','elf-tools','validator','output'):p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args();root=Path(__file__).resolve().parents[1];a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
c=json.loads(a.recipe.read_text());old=next(x[:-len('/Apps/springboard.c')] for x in c if x.endswith('/Apps/springboard.c'))
c=[x.replace(old,str(root)) for x in c];elf=a.output/'springboard.elf';c[c.index('-o')+1]=str(elf)
assert '-DPORTABLE_WATCH_NATIVE_HEIGHT' in c and '-DPORTABLE_ASYNC_RGB565' in c
(a.output/'command.json').write_text(json.dumps(c,indent=2)+'\n')
with (a.output/'target.log').open('w') as log:subprocess.run(c,stdout=log,stderr=subprocess.STDOUT,check=True)
sys.path.insert(0,str(a.elf_tools.resolve()))
from normalize_xtensa_relocations import normalize
from compact_current_elf import compact
normalize(elf);compaction=compact(elf,c[0],debug_path=a.output/'springboard.debug.elf')
subprocess.run([str(a.validator.resolve()),str(elf)],check=True)
proof={'system_source':subprocess.check_output(['git','-C',root,'rev-parse','HEAD'],text=True).strip(),'sha256':hashlib.sha256(elf.read_bytes()).hexdigest(),'bytes':elf.stat().st_size,'version':'1.7.12','compaction':compaction,'recipe_sha256':hashlib.sha256(a.recipe.read_bytes()).hexdigest()}
(a.output/'build.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps(proof,indent=2))

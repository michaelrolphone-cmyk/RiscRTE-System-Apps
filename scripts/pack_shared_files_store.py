#!/usr/bin/env python3
"""Lossless loader-preserving ELF compaction and exact bootfs round-trip proof."""
import argparse,hashlib,json,shutil,sys
from pathlib import Path
p=argparse.ArgumentParser()
for name in ('store','tools','output','compiler'):p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();out=a.output.resolve()
if out.exists():raise ValueError('Output must be new; source store is immutable')
sys.path.insert(0,str(a.tools.resolve()/'scripts'))
from compact_current_elf import compact
from current_bootfs import build
shutil.copytree(a.store,out/'store');source={str(f.relative_to(a.store)):hashlib.sha256(f.read_bytes()).hexdigest() for f in a.store.rglob('*') if f.is_file()};proof={}
for file in sorted((out/'store').rglob('*.elf')):
 name=str(file.relative_to(out/'store'));proof[name]=compact(file,str(a.compiler.resolve()),out/'debug'/name)
files={str(f.relative_to(out/'store')):f.read_bytes() for f in (out/'store').rglob('*') if f.is_file()}
image,packing=build(files)
assert set(files)==set(source)
assert all(hashlib.sha256(data).hexdigest()==source[name] for name,data in files.items() if not name.endswith('.elf'))
assert all(v['retained_loader_sections_symbols_relocations_unchanged'] for v in proof.values())
(out/'bootfs.bin').write_bytes(image)
report=dict(status='BOOT_STORE_ONLY_NOT_FIRMWARE',source_store_sha256=source,store_sha256={k:hashlib.sha256(v).hexdigest() for k,v in files.items()},compaction=proof,packing=packing,tools_commit=__import__('subprocess').check_output(['git','-C',str(a.tools),'rev-parse','HEAD'],text=True).strip())
(out/'packing.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(packing,indent=2))

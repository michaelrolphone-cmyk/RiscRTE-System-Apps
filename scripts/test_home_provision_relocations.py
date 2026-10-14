#!/usr/bin/env python3
"""Qualify the exact observed sectioned-Home placeholder, plus rejection cases."""
import argparse,importlib.util,io,struct,tempfile
from pathlib import Path
from elftools.elf.elffile import ELFFile
from normalize_home_provision import normalize
p=argparse.ArgumentParser();p.add_argument('--before',type=Path,required=True);p.add_argument('--canonical',type=Path,required=True);a=p.parse_args()
raw=a.before.read_bytes();elf=ELFFile(io.BytesIO(raw));section=elf.get_section_by_name('.rela.dyn');rows=list(section.iter_relocations());start=section['sh_offset'];count=len(rows)
assert rows[-5]['r_info_type']==0
spec=importlib.util.spec_from_file_location('canonical',a.canonical);mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
with tempfile.TemporaryDirectory() as td:
 out=Path(td)/'test.elf';out.write_bytes(raw);assert normalize(out)==1;assert mod.normalize(out)==1
 fixed=out.read_bytes();assert normalize(out)==0 and out.read_bytes()==fixed
 after=ELFFile(io.BytesIO(fixed));kept=list(after.get_section_by_name('.rela.dyn').iter_relocations())
 assert [dict(r.entry) for r in kept]==[dict(r.entry) for r in rows if r['r_info_type']]
 for name in ('.text','.rodata','.data','.data.rel.ro','.got','.rela.plt'):
  assert elf.get_section_by_name(name).data()==after.get_section_by_name(name).data(),name
 for mode in range(6):
  bad=bytearray(raw)
  if mode==0:struct.pack_into('<I',bad,start+(count-5)*12+8,1) # nonzero no-op payload
  if mode==1:struct.pack_into('<I',bad,start+(count-4)*12+8,3) # unknown RTLD addend
  if mode==2:struct.pack_into('<I',bad,start+(count-4)*12,1) # unaligned target
  if mode==3:struct.pack_into('<I',bad,start+(count-4)*12+4,5) # wrong tail type
  if mode==4:bad[start:start+12]=bytes(12) # extra zero
  if mode==5:struct.pack_into('<I',bad,start+4,6) # unsupported relocation
  out.write_bytes(bad)
  try:normalize(out)
  except ValueError:pass
  else:raise AssertionError(mode)
  assert out.read_bytes()==bad
print('PASS: exact real relocations/code preserved; canonical validation input; idempotent final ELF; six malformed patterns rejected unchanged')

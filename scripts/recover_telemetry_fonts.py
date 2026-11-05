#!/usr/bin/env python3
"""Recover OFL telemetry raster source from the qualified X4 .114 scene ELF.
No target instructions are changed. This avoids host FreeType raster differences.
"""
import argparse,hashlib,io,struct
from pathlib import Path
from elftools.elf.elffile import ELFFile
p=argparse.ArgumentParser();p.add_argument('--baseline-elf',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
raw=a.baseline_elf.read_bytes()
assert hashlib.sha256(raw).hexdigest()=='a264c572f180d53e6f8e4632bd2834e5dc0b3a3581c95e5fe9995197deea4359'
elf=ELFFile(io.BytesIO(raw));symbols={s.name:s for s in elf.get_section_by_name('.symtab').iter_symbols()}
def data(name):
 s=symbols[name];sec=elf.get_section(s['st_shndx']);offset=s['st_value']-sec['sh_addr'];return sec.data()[offset:offset+s['st_size']]
lines=['/* Qualified X4 .114 telemetry rasters; recovered OFL-1.1 font data. */']
for scale in (1,2):
 for role in range(10):
  name=f'tmfont_{scale}_{role}';table=data(name+'_glyphs');assert len(table)==95*12
  glyphs=[]
  for cp in range(32,127):
   symbol=f'{name}_{cp}';bits=data(symbol);width,height,left,top,advance=struct.unpack_from('<BBbbH',table,(cp-32)*12)
   assert len(bits)==max(1,(width*height+7)//8)
   lines.append('static const uint8_t '+symbol+'[]={'+','.join(map(str,bits))+'};')
   glyphs.append('{%d,%d,%d,%d,%d,%s}'%(width,height,left,top,advance,symbol))
  lines+=['static const pc_glyph '+name+'_glyphs[]={'+','.join(glyphs)+'};','static const pc_font '+name+'={'+name+'_glyphs,NULL,0};']
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text('\n'.join(lines)+'\n')
print(hashlib.sha256(a.output.read_bytes()).hexdigest())

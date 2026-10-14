#!/usr/bin/env python3
"""Move the observed GCC8 sectioned Home null relocation to the tail.

The canonical Reader normalizer then removes that trailing no-op. No sections,
addresses, instructions or nonzero relocations are changed or removed.
"""
from pathlib import Path
import io
from elftools.elf.elffile import ELFFile

def normalize(path):
 path=Path(path);data=bytearray(path.read_bytes());elf=ELFFile(io.BytesIO(data))
 if elf.elfclass!=32 or not elf.little_endian or elf['e_machine']!='EM_XTENSA' or elf['e_type']!='ET_DYN':raise ValueError('Expected Xtensa ELF32 shared object')
 sections=list(elf.iter_sections())
 if len({s.name for s in sections})!=len(sections):raise ValueError('Duplicate sections')
 rela=elf.get_section_by_name('.rela.dyn')
 if rela is None or rela['sh_type']!='SHT_RELA' or rela['sh_entsize']!=12:raise ValueError('Expected .rela.dyn records')
 rows=list(rela.iter_relocations());blank=[i for i,r in enumerate(rows) if r['r_info_type']==0]
 if not blank:return 0
 if blank!=[len(rows)-5]:raise ValueError('Unrecognized Home no-op layout')
 def mapped(addr):return addr and addr%4==0 and any(s['sh_flags']&2 and s['sh_addr']<=addr and addr+4<=s['sh_addr']+s['sh_size'] for s in sections)
 tail=rows[-4:]
 if [r['r_addend'] for r in tail]!=[1,2,1,2] or any(r['r_info']!=2 or not mapped(r['r_offset']) for r in tail):raise ValueError('Unexpected loader relocation tail')
 if any(r['r_info_type']!=5 or r['r_info_sym'] or not mapped(r['r_offset']) for r in rows[:-5]):raise ValueError('Unexpected preceding Home relocations')
 start=rela['sh_offset']+blank[0]*12;end=rela['sh_offset']+rela['sh_size']
 if data[start:start+12]!=bytes(12):raise ValueError('No-op record contains data')
 before=bytes(data);data[start:end]=data[start+12:end]+bytes(12)
 assert bytes(data[:start])==before[:start] and bytes(data[end:])==before[end:]
 assert bytes(data[start:end-12])==before[start+12:end]
 path.write_bytes(data);return 1
if __name__=='__main__':
 import argparse
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('elf',type=Path);a=p.parse_args()
 print('Moved exact sectioned Home no-op:',normalize(a.elf))

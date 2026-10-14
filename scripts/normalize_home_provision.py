#!/usr/bin/env python3
"""Move only the observed GCC8 sectioned Home null relocation to the tail.

The single all-zero slot may precede the four RTLD records or already trail
those records. The canonical Reader normalizer removes the trailing no-op.
No sections, addresses, instructions or real relocation records are removed.
"""
import io
from pathlib import Path

from elftools.elf.elffile import ELFFile


def normalize(path):
    path = Path(path)
    data = bytearray(path.read_bytes())
    elf = ELFFile(io.BytesIO(data))
    if (elf.elfclass != 32 or not elf.little_endian or
            elf['e_machine'] != 'EM_XTENSA' or elf['e_type'] != 'ET_DYN'):
        raise ValueError('Expected Xtensa ELF32 shared object')
    sections = list(elf.iter_sections())
    if len({section.name for section in sections}) != len(sections):
        raise ValueError('Duplicate sections')
    rela = elf.get_section_by_name('.rela.dyn')
    if (rela is None or rela['sh_type'] != 'SHT_RELA' or
            rela['sh_entsize'] != 12 or rela['sh_size'] % 12 or
            rela['sh_offset'] % 4 or rela['sh_addr'] % 4 or
            rela['sh_offset'] + rela['sh_size'] > len(data)):
        raise ValueError('Expected .rela.dyn records')
    dynamic = elf.get_section_by_name('.dynamic')
    if (dynamic is None or dynamic['sh_type'] != 'SHT_DYNAMIC' or
            dynamic['sh_entsize'] != 8 or dynamic['sh_size'] % 8 or
            dynamic['sh_offset'] % 4 or
            dynamic['sh_offset'] + dynamic['sh_size'] > len(data)):
        raise ValueError('Expected dynamic records')
    # Neither normalizer may modify bytes aliased as code or ELF metadata.
    headers = [(0, elf['e_ehsize']),
               (elf['e_phoff'], elf['e_phoff'] + elf['e_phnum'] * elf['e_phentsize']),
               (elf['e_shoff'], elf['e_shoff'] + elf['e_shnum'] * elf['e_shentsize'])]
    for target in (rela, dynamic):
        begin, end = target['sh_offset'], target['sh_offset'] + target['sh_size']
        other_ranges = [(section['sh_offset'], section['sh_offset'] + section['sh_size'])
                        for section in sections if section.name != target.name and
                        section['sh_type'] != 'SHT_NOBITS' and section['sh_size']]
        if any(lo < hi and begin < hi and lo < end for lo, hi in [*headers, *other_ranges]):
            raise ValueError('Overlapping ' + target.name + ' data')
    tags = list(dynamic.iter_tags())
    for name, value in (('DT_RELA', rela['sh_addr']),
                        ('DT_RELASZ', rela['sh_size']), ('DT_RELAENT', 12)):
        found = [tag['d_val'] for tag in tags if tag['d_tag'] == name]
        if found != [value]:
            raise ValueError('Inconsistent ' + name)
    rows = list(rela.iter_relocations())
    blank = [i for i, row in enumerate(rows) if row['r_info_type'] == 0]
    if blank and blank not in ([len(rows) - 5], [len(rows) - 1]):
        raise ValueError('Unrecognized Home no-op layout')
    start, end = rela['sh_offset'], rela['sh_offset'] + rela['sh_size']
    if blank and data[start + blank[0] * 12:start + (blank[0] + 1) * 12] != bytes(12):
        raise ValueError('No-op record contains data')
    real = [row for row in rows if row['r_info_type'] != 0]

    def mapped(address):
        return address and address % 4 == 0 and any(
            section['sh_flags'] & 2 and section['sh_addr'] <= address and
            address + 4 <= section['sh_addr'] + section['sh_size']
            for section in sections)

    if (len(real) < 5 or [row['r_addend'] for row in real[-4:]] != [1, 2, 1, 2] or
            any(row['r_info'] != 2 or not mapped(row['r_offset']) for row in real[-4:])):
        raise ValueError('Unexpected loader relocation tail')
    if any(row['r_info'] != 5 or not mapped(row['r_offset']) for row in real[:-4]):
        raise ValueError('Unexpected preceding Home relocations')
    relacount = [tag['d_val'] for tag in tags if tag['d_tag'] == 'DT_RELACOUNT']
    if relacount != [len(real) - 4]:
        raise ValueError('Inconsistent DT_RELACOUNT')
    # The Linux GCC8 link already puts its slot at the tail. Validate it just
    # as strictly, but let the canonical remover handle it without rewriting.
    if not blank or blank == [len(rows) - 1]:
        return 0
    start += blank[0] * 12
    before = bytes(data)
    data[start:end] = data[start + 12:end] + bytes(12)
    assert bytes(data[:start]) == before[:start] and bytes(data[end:]) == before[end:]
    assert bytes(data[start:end - 12]) == before[start + 12:end]
    path.write_bytes(data)
    return 1


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('elf', type=Path)
    args = parser.parse_args()
    print('Moved exact sectioned Home no-op:', normalize(args.elf))

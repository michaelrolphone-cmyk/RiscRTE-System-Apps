#!/usr/bin/env python3
"""Qualify both observed Home no-op positions without changing real relocations."""
import argparse
import importlib.util
import io
import struct
import tempfile
from pathlib import Path

from elftools.elf.elffile import ELFFile
from elftools.common.exceptions import ELFError
from normalize_home_provision import normalize


def qualify(raw, canonical):
    elf = ELFFile(io.BytesIO(raw))
    sections = list(elf.iter_sections())
    section = elf.get_section_by_name('.rela.dyn')
    rows = list(section.iter_relocations())
    start, size = section['sh_offset'], section['sh_size']
    blank = [i for i, row in enumerate(rows) if row['r_info_type'] == 0]
    assert blank in ([len(rows) - 5], [len(rows) - 1]), blank
    assert raw[start + blank[0] * 12:start + (blank[0] + 1) * 12] == bytes(12)
    real = [raw[start + i * 12:start + (i + 1) * 12]
            for i in range(len(rows)) if i not in blank]
    rela_header = elf['e_shoff'] + sections.index(section) * elf['e_shentsize']
    dynamic = elf.get_section_by_name('.dynamic')
    dynamic_offsets = {}
    for pos in range(dynamic['sh_offset'], dynamic['sh_offset'] + dynamic['sh_size'], 8):
        tag, _ = struct.unpack_from('<II', raw, pos)
        dynamic_offsets.setdefault(tag, []).append(pos)
    assert len(dynamic_offsets[8]) == 1
    expected = bytearray(raw)
    expected[start:start + size] = b''.join(real) + bytes(12)
    struct.pack_into('<I', expected, rela_header + 20, size - 12)
    struct.pack_into('<I', expected, dynamic_offsets[8][0] + 4, size - 12)
    rejected = 0
    with tempfile.TemporaryDirectory() as temporary:
        path = Path(temporary) / 'home.elf'
        for position in (len(rows) - 5, len(rows) - 1):
            records = real.copy()
            records.insert(position, bytes(12))
            variant = bytearray(raw)
            variant[start:start + size] = b''.join(records)
            path.write_bytes(variant)
            assert normalize(path) == (1 if position == len(rows) - 5 else 0)
            moved = path.read_bytes()
            # The adapter itself is idempotent, before canonical removal too.
            assert normalize(path) == 0 and path.read_bytes() == moved
            assert canonical.normalize(path) == 1
            fixed = path.read_bytes()
            # Exact whole-file equality proves that no code, symbols, addresses,
            # program headers or other section payloads changed. Only the null
            # relocation and the two canonical size fields are excluded.
            assert fixed == bytes(expected)
            assert normalize(path) == 0 and path.read_bytes() == fixed
            kept = ELFFile(io.BytesIO(fixed)).get_section_by_name('.rela.dyn')
            assert [dict(row.entry) for row in kept.iter_relocations()] == [
                dict(row.entry) for row in rows if row['r_info_type']]
            tail = len(rows) - 4 if position == len(rows) - 5 else len(rows) - 5
            mutations = [
                ('nonzero no-op addend', start + position * 12 + 8, 1),
                ('nonzero no-op symbol', start + position * 12 + 4, 256),
                ('unknown RTLD addend', start + tail * 12 + 8, 3),
                ('unaligned RTLD destination', start + tail * 12, 1),
                ('unmapped RTLD destination', start + tail * 12, 0xfffffffc),
                ('wrong RTLD type', start + tail * 12 + 4, 5),
                ('nonzero RTLD symbol', start + tail * 12 + 4, 258),
                ('unsupported preceding relocation', start + 4, 6),
                ('nonzero relative symbol', start + 4, 261),
                ('unaligned relative destination', start, 1),
                ('unmapped relative destination', start, 0xfffffffc),
                ('partial relocation row', rela_header + 20, size + 1),
                ('invalid relocation entry size', rela_header + 36, 8),
                ('out-of-file relocation data', rela_header + 16, len(raw)),
                ('code aliases relocations', elf['e_shoff'] + sections.index(elf.get_section_by_name('.text')) * elf['e_shentsize'] + 16, start),
                ('code aliases dynamic data', elf['e_shoff'] + sections.index(elf.get_section_by_name('.text')) * elf['e_shentsize'] + 16, dynamic['sh_offset']),
                ('inconsistent DT_RELA', dynamic_offsets[7][0] + 4, 0),
                ('inconsistent DT_RELASZ', dynamic_offsets[8][0] + 4, size - 12),
                ('inconsistent DT_RELAENT', dynamic_offsets[9][0] + 4, 8),
                ('inconsistent DT_RELACOUNT', dynamic_offsets[0x6ffffff9][0] + 4, 0),
            ]
            bad_cases = []
            for name, offset, value in mutations:
                bad = variant.copy()
                struct.pack_into('<I', bad, offset, value)
                bad_cases.append((name, bad))
            bad = variant.copy()
            bad[start:start + 12] = bytes(12)
            bad_cases.append(('extra zero record', bad))
            bad = variant.copy()
            # Preserve one zero, but put it inside the RELATIVE block.
            bad[start:start + 12], bad[start + position * 12:start + (position + 1) * 12] = (
                bytes(12), bytes(bad[start:start + 12]))
            bad_cases.append(('unobserved zero position', bad))
            for name, bad in bad_cases:
                path.write_bytes(bad)
                try:
                    normalize(path)
                except (ValueError, ELFError):
                    pass
                else:
                    raise AssertionError(name)
                assert path.read_bytes() == bad, name
                rejected += 1
    print(f'PASS: both no-op positions; exact real relocations and whole-file preservation; '
          f'idempotent adapter/final ELF; {rejected} malformed layouts rejected unchanged')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--before', type=Path, required=True)
    parser.add_argument('--canonical', type=Path, required=True)
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location('canonical', args.canonical)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    qualify(args.before.read_bytes(), module)

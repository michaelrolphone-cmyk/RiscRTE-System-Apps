#!/usr/bin/env python3
"""Compare client C/native C++ catalog ABI with the actual Xtensa compiler."""
from pathlib import Path
import argparse
import json
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--toolchain-bin', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    runtime = args.runtime.resolve()
    revision = subprocess.check_output(
        ['git', '-C', str(runtime), 'rev-parse', 'HEAD'], text=True).strip()
    if revision != 'c258c4ba144247b6eaa4c6f28675ffccec582fe1':
        parser.error('runtime must be pinned to the published .39/.41 native source')
    output = (args.output or Path(tempfile.mkdtemp(prefix='t063-abi-'))).resolve()
    output.mkdir(parents=True, exist_ok=True)
    prefix = str(args.toolchain_bin.resolve()/'xtensa-esp32s3-elf-')
    expected = json.loads((ROOT/'target-catalog-abi.json').read_text())
    for name, compiler, lang, header in (
        ('client', 'gcc', 'c', ROOT.parents[2]/'lib/PortableApps/include'),
        ('native', 'g++', 'c++', runtime/'sdk/app'),
    ):
        obj = output/(name+'.o')
        binary = output/(name+'.bin')
        subprocess.run([prefix+compiler, '-x', lang, '-O2', '-I'+str(header),
                        '-c', str(ROOT/'target-abi-probe.c'), '-o', str(obj)], check=True)
        subprocess.run([prefix+'objcopy', '-O', 'binary', '-j', '.rodata',
                        str(obj), str(binary)], check=True)
        values = list(struct.unpack('<8I', binary.read_bytes()))
        if values != expected['values'][name]:
            raise RuntimeError(f'{name} catalog ABI mismatch: {values}')
        print(f'{name}: {values}')
    if (output/'client.bin').read_bytes() != (output/'native.bin').read_bytes():
        raise RuntimeError('C and C++ catalog layouts differ')
    print('PASS: target layouts match; no target instructions executed')


if __name__ == '__main__':
    main()

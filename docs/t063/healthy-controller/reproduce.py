#!/usr/bin/env python3
"""Rebuild the healthy-provider full-controller diagnostic investigation."""
from pathlib import Path
import argparse
import json
import os
import subprocess
import tempfile

FIXTURE = Path(__file__).resolve().parent
BRIDGE_COMMIT = '6e82fdcaec0b73ed700d2104cfb4d94f011c19d4'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bridges', type=Path, required=True)
    parser.add_argument('--system', type=Path, default=FIXTURE.parents[2])
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    bridge = args.bridges.resolve()
    actual = subprocess.check_output(
        ['git', '-C', str(bridge), 'rev-parse', 'HEAD'], text=True).strip()
    if actual != BRIDGE_COMMIT:
        parser.error(f'bridges must be pinned to {BRIDGE_COMMIT}; got {actual}')
    output = (args.output or Path(tempfile.mkdtemp(prefix='t063-healthy-'))).resolve()
    output.mkdir(parents=True, exist_ok=True)
    paths = dict(fixture=str(FIXTURE), system=str(args.system.resolve()),
                 bridge=str(bridge), output=str(output))
    for name in ('command24.json', 'command24-sanitized.json'):
        command = [arg.format_map(paths) for arg in json.loads((FIXTURE/name).read_text())]
        subprocess.run(command, check=True)
        binary = command[command.index('-o')+1]
        for display in ('sync', 'async'):
            result = subprocess.run(
                [binary, '410', '502', '-', display, 'eligible'],
                env={**os.environ, 'RUN_ALL': '1', 'ASAN_OPTIONS': 'detect_leaks=0'},
                check=True, text=True, capture_output=True, timeout=90)
            (output/(Path(binary).name+'-'+display+'.log')).write_text(
                result.stdout+result.stderr)
            print(result.stdout.splitlines()[-2])
    print(f'Four controller cases passed. Logs: {output}')


if __name__ == '__main__':
    main()

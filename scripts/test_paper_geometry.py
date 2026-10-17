#!/usr/bin/env python3
"""Check signed paper endpoint projection against actual inverse input arithmetic."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/paper-geometry'
OUT.mkdir(parents=True, exist_ok=True)
sources = ['lib/PortableApps/include/PortablePaperGeometry.h',
           'test/native_apps/paper_geometry_test.c', 'scripts/test_paper_geometry.py']
results = []
for sanitized in (False, True):
    binary = OUT / ('sanitized' if sanitized else 'normal')
    flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
             '-fno-omit-frame-pointer'] if sanitized else []
    if sanitized and sys.platform != 'darwin':
        flags += ['-no-pie']
    command = [os.environ.get('CC', 'cc'), '-std=c11', '-O2', '-Wall', '-Wextra',
               '-Werror', *flags, '-I'+str(ROOT/'lib/PortableApps/include'),
               str(ROOT/'test/native_apps/paper_geometry_test.c'), '-o', str(binary)]
    subprocess.run(command, check=True)
    result = subprocess.run([str(binary)], check=True, capture_output=True, text=True,
                            env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0'))
    print(result.stdout.strip())
    results.append({'sanitized': sanitized, 'command': command,
                    'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
                    'stdout': result.stdout, 'stderr': result.stderr})
    binary.unlink()
(OUT/'results.json').write_text(json.dumps({'results': results,
    'source_sha256': {p: hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in sources}},
    indent=2)+'\n')

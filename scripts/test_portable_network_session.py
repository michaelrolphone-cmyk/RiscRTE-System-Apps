#!/usr/bin/env python3
"""Compile the actual saved-network helper and profile codec with fault fixtures."""
import os
from pathlib import Path
import subprocess
import sys
ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/portable-network-session'
OUT.mkdir(parents=True, exist_ok=True)
for sanitized in (False, True):
    flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer'] if sanitized else []
    if sanitized and sys.platform != 'darwin': flags += ['-no-pie']
    binary = OUT / ('network-session-san' if sanitized else 'network-session')
    subprocess.run([os.environ.get('CC','cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', *flags,
        '-I'+str(ROOT/'lib/PortableApps/include'), str(ROOT/'lib/PortableApps/src/PortableNetworkSession.c'),
        str(ROOT/'test/native_apps/portable_network_session_test.c'), '-o', str(binary)], check=True, timeout=60)
    subprocess.run([str(binary)], check=True, timeout=20)
print('Normal and ASan/UBSan network-session fixtures passed; no device or network access.')

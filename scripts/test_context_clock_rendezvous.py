#!/usr/bin/env python3
"""Run actual Home Contexts rendezvous/client code in resident and legacy modes."""
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
out = root / 'build/context-clock-rendezvous'
out.mkdir(parents=True, exist_ok=True)
cases = ('normal reload disabled ready unsupported unavailable unfinished '
         'owner-unfinished launch-refused status-failed bad-size cleanup-pending '
         'unknown-pending unknown-active request-failed begin-failed '
         'release-failed finish-failed').split()
for resident in (False, True):
    for sanitized in (False, True):
        flags = ['-DTEST_CONTEXTS_RESIDENT'] if resident else []
        if sanitized:
            flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                      '-fno-omit-frame-pointer', '-no-pie']
        binary = out / f'rendezvous-{int(resident)}-{int(sanitized)}'
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g',
                        '-Wall', '-Wextra', '-Werror', *flags,
                        '-I' + str(root / 'lib/PortableApps/include'),
                        str(root / 'test/native_apps/context_clock_rendezvous_test.c'),
                        '-o', str(binary)], check=True)
        for case in cases:
            subprocess.run([str(binary), case], check=True, timeout=20,
                           env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0',
                                    UBSAN_OPTIONS='halt_on_error=1'))
print('72 Home Contexts legacy/resident rendezvous scenarios normal + ASan/UBSan PASS')

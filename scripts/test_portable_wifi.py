#!/usr/bin/env python3
"""Wi-Fi fake-provider tests; never touches a host network or user credentials."""
import os
import sys
from pathlib import Path
import subprocess
ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/portable'
OUT.mkdir(parents=True, exist_ok=True)
for sanitizer in (False, True):
    flags = ['-fsanitize='+os.environ.get('WIFI_SANITIZERS','address,undefined'), '-fno-omit-frame-pointer', '-no-pie'] if sanitizer else []
    if sys.platform == 'darwin': flags = [f for f in flags if f != '-no-pie']
    environment = os.environ.copy()
    # Inherit sanitizer policy: hosted CI retains LeakSanitizer. A ptrace-based
    # local executor may explicitly set ASAN_OPTIONS=detect_leaks=0.
    for fixture in ('portable_wifi_credentials_test', 'portable_wifi_saved_network_test'):
        binary = OUT / (fixture + ('-san' if sanitizer else ''))
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
                        *flags, '-I'+str(ROOT/'lib/PortableApps/include'),
                        str(ROOT/'test/native_apps'/f'{fixture}.c'), '-o', str(binary)], check=True, timeout=60)
        subprocess.run([str(binary)], check=True, timeout=20, env=environment)
    for nova in (False, True):
        for rotation in (0, 180):
            binary = OUT / (f'wifi-{int(nova)}-{rotation}' + ('-san' if sanitizer else ''))
            subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
                            *flags, *(['-DPORTABLE_NOVA_UI'] if nova else []), f'-DPORTABLE_TOUCH_ROTATION={rotation}',
                            '-I'+str(ROOT/'lib/PortableApps/include'), '-I'+str(ROOT/'lib/NativeApps/include'),
                            str(ROOT/'test/native_apps/portable_wifi_test.c'), '-o', str(binary)], check=True, timeout=60)
            for scenario in range(48):
                subprocess.run([str(binary), str(scenario)], check=True, timeout=10, env=environment)
print('Portable Wi-Fi: 48 app scenarios x 2 UI profiles x 2 orientations x 2 compiler modes; credential and saved-client fault suites passed')

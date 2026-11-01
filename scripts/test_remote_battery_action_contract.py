#!/usr/bin/env python3
"""Keep ordinary Quick declarations unchanged and remote action values opt-in."""
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
REMOTE = ('OPEN', 'BACK', 'SCAN', 'STOP', 'SELECT', 'PREVIOUS', 'NEXT')
ORDINARY = ('BRIGHTNESS_PREVIEW', 'BRIGHTNESS_COMMIT', 'VOLUME_COMMIT',
            'SILENT', 'TORCH', 'WIFI', 'AIRPLANE', 'BLUETOOTH', 'DND',
            'USB_TRANSFER', 'CLEAN_REFRESH', 'TONE_PREVIEW', 'TONE_COMMIT', 'CONTEXTS')


def compile_source(source, enabled):
    command = [os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra',
               '-Werror', '-fsyntax-only', '-x', 'c',
               '-I' + str(ROOT / 'lib/PortableApps/include')]
    if enabled:
        command.append('-DPORTABLE_QUICK_REMOTE_BATTERY')
    return subprocess.run([*command, '-'], input=source, text=True, capture_output=True)


prefix = '#include "PortableQuickActions.h"\n'
ordinary = '_Static_assert(PQA_NONE == 0, "none");\n'
ordinary += ''.join('_Static_assert(PQA_' + name + ' == (1u << ' + str(bit)
                    + '), "ordinary action value");\n'
                    for bit, name in enumerate(ORDINARY))
remote = ''.join('_Static_assert(PQA_REMOTE_' + name + ' == (1u << ' + str(bit)
                  + '), "remote action value");\n'
                  for bit, name in enumerate(REMOTE, 14))
for enabled in (False, True):
    result = compile_source(prefix + ordinary + (remote if enabled else ''), enabled)
    assert result.returncode == 0, result.stderr
for name in REMOTE:
    result = compile_source(prefix + 'unsigned value = PQA_REMOTE_' + name + ';\n', False)
    assert result.returncode != 0 and 'PQA_REMOTE_' + name in result.stderr, name
print('Quick action contract: ordinary values preserved, seven remote values unchanged when enabled and absent when disabled')

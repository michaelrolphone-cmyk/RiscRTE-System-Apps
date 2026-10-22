#!/usr/bin/env python3
"""Pure opt-in remote battery intent: normal/ASan+UBSan, tone off/on."""
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build' / 'quick-remote-controls'
CASES = (
    'idle', 'summary', 'explicit-intent', 'hit-map', 'rows-navigation',
    'drag-cancel', 'cancel-gate', 'epoch-snapshot', 'closed-moving',
    'disabled', 'page-invalidated', 'selection-invalidated',
)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    failures = []
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1')
    for tone in (False, True):
        for sanitizer in (False, True):
            target = OUT / f'controls-tone-{int(tone)}-san-{int(sanitizer)}'
            flags = [
                '-DPORTABLE_QUICK_REMOTE_BATTERY', '-DPORTABLE_RESIDENT_SHELL_HOST',
                '-DPORTABLE_PAPER_TRANSITIONS', '-DPORTABLE_QUICK_USB_TRANSFER',
            ]
            if tone:
                flags.append('-DPORTABLE_FRONTLIGHT_TONE')
            if sanitizer:
                flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                          '-fno-omit-frame-pointer']
                if sys.platform != 'darwin':
                    flags.append('-no-pie')
            subprocess.run([
                os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall',
                '-Wextra', '-Werror', '-pedantic', *flags,
                '-I' + str(ROOT / 'lib/PortableApps/include'),
                str(ROOT / 'lib/PortableApps/src/quick_actions.c'),
                str(ROOT / 'test/native_apps/quick_remote_controls_test.c'),
                '-o', str(target),
            ], check=True, timeout=120)
            # Separate processes expose every failed invariant without masking it.
            for case in CASES:
                result = subprocess.run([str(target), case], env=env, timeout=20, check=False)
                if result.returncode:
                    failures.append(f'{target.name}: {case} (exit {result.returncode})')
    if failures:
        print('\nRemote Quick Actions failures:\n' + '\n'.join(failures), file=sys.stderr)
        return 1
    print(f'{len(CASES) * 4} remote Quick Actions pure-controller cases passed; '
          'normal/ASan+UBSan, tone off/on, no runtime or radio linkage')
    return 0


if __name__ == '__main__':
    sys.exit(main())

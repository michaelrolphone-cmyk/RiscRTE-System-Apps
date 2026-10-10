#!/usr/bin/env python3
"""Production optional-tone session with strict hardware-independent providers."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--display-sdk', type=Path, required=True,
                        help='SDK containing display base/history and power headers')
    parser.add_argument('--tone-sdk', type=Path, required=True,
                        help='SDK containing optional frontlight, snapshot and metrics headers')
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'build/frontlight-tone-session')
    args = parser.parse_args()
    out = args.output_dir.resolve()
    include = out / 'include'
    shutil.copytree(ROOT / 'lib/PortableApps/include', include, dirs_exist_ok=True)
    for name in ['RiscDisplayOutputV1.h', 'RiscDisplayOutputPowerV1.h']:
        shutil.copyfile(args.display_sdk / name, include / name)
    for name in ['RiscDisplayOutputMetricsV1.h', 'RiscDisplayOutputSnapshotV1.h',
                 'RiscDisplayOutputFrontlightV1.h']:
        shutil.copyfile(args.tone_sdk / name, include / name)
    for sanitized in (False, True):
        binary = out / ('session-san' if sanitized else 'session')
        flags = []
        if sanitized:
            flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                      '-fno-omit-frame-pointer']
            if sys.platform != 'darwin':
                flags += ['-no-pie']
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g',
                        '-Wall', '-Wextra', '-Werror', '-pedantic', *flags,
                        '-DPORTABLE_FRONTLIGHT_TONE', '-DPORTABLE_PAPER_TRANSITIONS',
                        '-DPORTABLE_RESIDENT_SHELL_HOST', '-I' + str(include),
                        ROOT / 'test/native_apps/frontlight_tone_session_test.c',
                        ROOT / 'lib/PortableApps/src/quick_actions.c',
                        ROOT / 'lib/PortableApps/src/quick_session.c', '-o', binary], check=True)
        subprocess.run([binary], check=True, timeout=30,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()

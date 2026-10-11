#!/usr/bin/env python3
"""Run bounded checked crash-report SD export fault tests (normal + ASan/UBSan)."""
import argparse
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, default=ROOT/'build/crash-report-sd')
    args = parser.parse_args()
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    for sanitized in (False, True):
        name = 'sanitized' if sanitized else 'normal'
        binary = out/('crash-report-sd-'+name)
        flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                 '-fno-omit-frame-pointer', '-no-pie'] if sanitized else []
        subprocess.run(['cc', '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                        *flags, '-I'+str(ROOT/'lib/PortableApps/include'),
                        str(ROOT/'test/native_apps/crash_report_sd_test.c'), '-o', str(binary)], check=True)
        result = subprocess.run([str(binary)], check=True, timeout=30, text=True, stdout=subprocess.PIPE,
                                env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
        print(name+' '+result.stdout.strip(), flush=True)


if __name__ == '__main__':
    main()

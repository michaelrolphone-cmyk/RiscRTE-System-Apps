#!/usr/bin/env python3
"""Test the shared crash-report body against an explicit Runtime v1 SDK."""
import argparse
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, default=ROOT/'build/failure-evidence-report')
    args = parser.parse_args()
    sdk = args.runtime.resolve()/'sdk/app'
    if not (sdk/'RiscFailureEvidenceV1.h').is_file():
        parser.error('Runtime SDK is missing RiscFailureEvidenceV1.h')
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    for sanitized in (False, True):
        name = 'sanitized' if sanitized else 'normal'
        binary = out/('failure-evidence-report-'+name)
        flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                 '-fno-omit-frame-pointer', '-no-pie'] if sanitized else []
        subprocess.run(['cc', '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                        *flags, '-I'+str(ROOT/'lib/PortableApps/include'), '-I'+str(sdk),
                        str(ROOT/'test/native_apps/failure_evidence_report_test.c'), '-o', str(binary)], check=True)
        result = subprocess.run([str(binary)], check=True, timeout=30, text=True, stdout=subprocess.PIPE,
                                env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
        print(name+' '+result.stdout.strip(), flush=True)


if __name__ == '__main__':
    main()

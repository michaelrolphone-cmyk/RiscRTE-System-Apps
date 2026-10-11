#!/usr/bin/env python3
"""Test the bounded crash-report AppData spool, normally and under ASan/UBSan."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'build/crash-report-spool')
    parser.add_argument('--normal-only', action='store_true')
    args = parser.parse_args()
    runtime = args.runtime.resolve()
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    header = ROOT / 'lib/PortableApps/include/CrashReportSpool.h'
    source = ROOT / 'test/native_apps/crash_report_spool_test.c'
    sdk = runtime / 'sdk/app/RiscAppDataV1.h'
    results = []
    for sanitized in ([False] if args.normal_only else [False, True]):
        mode = 'sanitized' if sanitized else 'normal'
        binary = output / ('crash-report-spool-' + mode)
        flags = (['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                  '-fno-omit-frame-pointer', '-no-pie'] if sanitized else [])
        command = [os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g',
                   '-Wall', '-Wextra', '-Werror', '-pedantic', *flags,
                   '-I' + str(header.parent), '-I' + str(sdk.parent),
                   str(source), '-o', str(binary)]
        subprocess.run(command, check=True)
        # LSan cannot run under this executor's ptrace; ASan/UBSan still apply.
        result = subprocess.run([str(binary)], check=True, timeout=30,
                                text=True, stdout=subprocess.PIPE,
                                env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',
                                         UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
        print(mode + ': ' + result.stdout.strip(), flush=True)
        results.append({'mode': mode, 'command': command, 'result': result.stdout.strip()})
    (output / 'evidence.json').write_text(json.dumps({
        'hardware_tested': False,
        'backend': 'fake AppData with stat/read/atomic replace, revisions and injected faults',
        'runtime_sdk': str(sdk),
        'runtime_sdk_sha256': hashlib.sha256(sdk.read_bytes()).hexdigest(),
        'source_sha256': {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                          for path in (header, source, Path(__file__).resolve())},
        'runs': results,
    }, indent=2) + '\n')


if __name__ == '__main__':
    main()

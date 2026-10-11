#!/usr/bin/env python3
"""Exercise the actual Home failure archive lifecycle, normally and with ASan/UBSan.

The in-memory capability implementations enforce generation lifetimes, grant
ordering, durable acknowledgement and final SD verification before removal.
Physical OSD page presentation and the native Runtime broker have separate tests.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path)
    parser.add_argument('--receipt', type=Path, help='Optional additional small JSON receipt')
    args = parser.parse_args()
    sdk = args.runtime.resolve() / 'sdk/app'
    sdk_files = [sdk / name for name in ('RiscRuntimeV1.h', 'RiscFailureEvidenceV1.h',
                                       'RiscAppDataV1.h')]
    for path in sdk_files:
        if not path.is_file():
            parser.error(f'Runtime SDK is missing {path.name}')
    output = (args.output_dir.resolve() if args.output_dir else
              Path(tempfile.mkdtemp(prefix='failure-archive-lifecycle-')))
    output.mkdir(parents=True, exist_ok=True)
    source = ROOT / 'test/native_apps/failure_archive_lifecycle_test.c'
    covered = [source, Path(__file__).resolve(), ROOT / 'lib/PortableApps/src/failure_archive.inc',
               *(ROOT / 'lib/PortableApps/include' / name for name in
                 ('FailureEvidenceReport.h', 'CrashReportSpool.h', 'CrashReportSd.h',
                  'RiscStorageVolumeV1.h', 'RiscStorageExportV1.h', 'RiscStorageVolumeStateV1.h'))]
    initial_hashes = {str(path.relative_to(ROOT)): sha256(path) for path in covered}
    runs = []
    for sanitized in (False, True):
        mode = 'sanitized' if sanitized else 'normal'
        binary = output / ('failure-archive-lifecycle-' + mode)
        flags = (['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                  '-fno-omit-frame-pointer', '-no-pie'] if sanitized else [])
        command = [os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g',
                   '-Wall', '-Wextra', '-Werror', '-pedantic', *flags,
                   '-I' + str(sdk), '-I' + str(ROOT / 'lib/PortableApps/include'),
                   str(source), '-o', str(binary)]
        compile_result = subprocess.run(command, text=True, capture_output=True)
        (output / (mode + '-compile.log')).write_text(compile_result.stdout + compile_result.stderr)
        compile_result.check_returncode()
        result = subprocess.run([str(binary)], timeout=30, text=True, capture_output=True,
                                env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',
                                         UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
        (output / (mode + '.log')).write_text(result.stdout + result.stderr)
        print(mode + ':\n' + result.stdout, end='', flush=True)
        if result.stderr:
            print(result.stderr, end='', flush=True)
        result.check_returncode()
        runs.append({'mode': mode, 'command': command, 'exit_code': result.returncode,
                     'summary': result.stdout.strip().splitlines()[-1],
                     'log': str(output / (mode + '.log'))})
    current_hashes = {str(path.relative_to(ROOT)): sha256(path) for path in covered}
    if initial_hashes != current_hashes:
        raise SystemExit('Sources changed during verification; rerun to bind both results to one source set')
    receipt = {
        'hardware_tested': False,
        'scope': 'Real included failure_archive.inc, shared formatter, AppData spool and SD exporter; '
                 'in-memory host capability boundaries with strict lifetimes and operation ordering',
        'excluded': 'Physical OSD page completion and native Runtime/AppData broker; tested separately',
        'runtime_sdk': str(sdk),
        'sdk_sha256': {path.name: sha256(path) for path in sdk_files},
        'source_sha256': current_hashes,
        'runs': runs,
    }
    payload = json.dumps(receipt, indent=2) + '\n'
    (output / 'evidence.json').write_text(payload)
    if args.receipt:
        args.receipt.parent.mkdir(parents=True, exist_ok=True)
        args.receipt.write_text(payload)
    print('Evidence: ' + str(output / 'evidence.json'))


if __name__ == '__main__':
    main()

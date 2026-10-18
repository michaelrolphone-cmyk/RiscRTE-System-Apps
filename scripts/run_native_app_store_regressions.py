#!/usr/bin/env python3
"""Retain exact-source receipts for the unchanged shared updater regression suites."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
RUNNERS = ('scripts/test_watch_updates_native_layout.py', 'scripts/test_portable_update.py')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_hashes():
    roots = ('Apps', 'lib/PortableApps', 'lib/NativeApps', 'Services/update', 'test/native_apps')
    paths = {p for root in roots for p in (ROOT/root).rglob('*')
             if p.is_file() and p.suffix in ('.c', '.cpp', '.h', '.inc', '.json')}
    paths.update(ROOT/p for p in RUNNERS)
    paths.add(Path(__file__).resolve())
    return {str(p.relative_to(ROOT)): sha(p) for p in sorted(paths)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    if out.exists() and any(out.iterdir()):
        parser.error('Use an empty output directory')
    out.mkdir(parents=True, exist_ok=True)
    sources = source_hashes()
    (out/'source-hashes.json').write_text(json.dumps(sources, indent=2)+'\n')
    commands = [[os.sys.executable, str(ROOT/RUNNERS[0]), '--output', str(out/'legacy-layout')],
                [os.sys.executable, str(ROOT/RUNNERS[1])]]
    receipt = {'schema': 1, 'status': 'RUNNING', 'commands': commands,
               'hardware_verified': False, 'source_sha256': sha(out/'source-hashes.json'),
               'repository_commit': subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip(),
               'normal_and_sanitized': True, 'legacy_layout_scenarios': 312,
               'paired_cohort_scenarios_per_mode': 46, 'service_scenarios_per_kind_and_mode': 29,
               'controller_scenarios_per_kind_rotation_view_and_mode': 38}
    try:
        for index, command in enumerate(commands):
            with (out/f'suite-{index}.log').open('w') as log:
                result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, timeout=1200,
                    env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0:abort_on_error=1',
                         'UBSAN_OPTIONS': 'halt_on_error=1', 'UPDATE_SANITIZERS': 'address,undefined'})
            result.check_returncode()
        if json.loads((out/'legacy-layout/results.json').read_text())['scenarios'] != 312:
            raise RuntimeError('Incomplete legacy geometry regression matrix')
        if source_hashes() != sources:
            raise RuntimeError('Source changed during regression run; rerun settled source')
        receipt['log_sha256'] = {f'suite-{i}.log': sha(out/f'suite-{i}.log') for i in range(2)}
        receipt['status'] = 'PASS'
    except Exception as error:
        receipt.update(status='FAIL', error=str(error))
        raise
    finally:
        (out/'receipt.json').write_text(json.dumps(receipt, indent=2)+'\n')
    print('312 legacy layout cases and complete paired/service/controller fault suites passed')


if __name__ == '__main__':
    main()

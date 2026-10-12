#!/usr/bin/env python3
"""Opt-in Contexts backend lifetime fences; host probes, never hardware proof.

Requires exact selected Runtime and Utilities checkouts. Optional baseline System
compares default-macro object bytes at O1/O2/Os; --target-cc adds target-object
comparison, without linking, executing, delivering or flashing a firmware image.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
HEADERS = ('PortableContextsClient.h', 'PortableContextFingerprints.h',
           'PortableFingerprintClock.h', 'PortableContextRules.h')

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def source(path):
    result = {'path': str(path)}
    for key, command in (('commit', ['rev-parse', 'HEAD']),
                         ('status', ['status', '--porcelain'])):
        value = subprocess.run(['git', '-C', str(path), *command],
                               text=True, capture_output=True)
        result[key] = value.stdout.strip() if value.returncode == 0 else None
    return result

def main(args):
    args.output.mkdir(parents=True, exist_ok=False)
    spec = importlib.util.spec_from_file_location('contexts_sdk',
        args.utilities / 'scripts/build_contexts_scene.py')
    stage = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(stage)
    receipt = {'scope': 'Host callback-loss probes and optional raw object parity; not product qualification',
        'sources': {'system': source(ROOT), 'utilities': source(args.utilities),
                    'watch_runtime': source(args.runtime), 'paper_runtime': source(args.x4_runtime)},
        'header_sha256': {name: digest(ROOT / 'lib/PortableApps/include' / name) for name in HEADERS},
        'test_sha256': digest(ROOT / 'test/contexts/backend_liveness_test.c'),
        'runner_sha256': digest(Path(__file__)),
        'asan_options': os.environ.get('ASAN_OPTIONS', 'platform-default'),
        'probes': [], 'default_object_parity': []}
    if args.baseline_system:
        receipt['sources']['baseline_system'] = source(args.baseline_system)
    with tempfile.TemporaryDirectory(prefix='contexts-backend-liveness-') as directory:
        tmp = Path(directory)
        for profile, runtime in (('watch', args.runtime), ('paper', args.x4_runtime)):
            include = stage.stage(runtime, ROOT, tmp / profile / 'patched')
            defines = ['-DPORTABLE_NATIVE_TIME_TOOLBAR'] if profile == 'paper' else []
            base_flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', *defines]
            for sanitized in ((False, True) if args.sanitize else (False,)):
                executable = tmp / profile / ('sanitized' if sanitized else 'normal')
                flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                         '-fno-omit-frame-pointer', '-no-pie'] if sanitized else []
                subprocess.run([args.cc, *base_flags, '-O1', *flags, '-I' + str(include),
                    str(ROOT / 'test/contexts/backend_liveness_test.c'), '-o', str(executable)], check=True)
                result = subprocess.run([str(executable)], check=True, capture_output=True, text=True)
                print(profile, 'sanitized' if sanitized else 'normal', result.stdout.strip(), flush=True)
                receipt['probes'].append({'profile': profile, 'sanitized': sanitized,
                                          'result': result.stdout.strip()})
            if not args.baseline_system:
                continue
            baseline_include = stage.stage(runtime, args.baseline_system, tmp / profile / 'original')
            compilers = [('host', args.cc, [])]
            if args.target_cc:
                compilers.append(('target', args.target_cc,
                    ['-mlongcalls', '-mtext-section-literals', '-ffunction-sections', '-fdata-sections']))
            for compiler_name, compiler, flags in compilers:
                for optimization in ('-O1', '-O2', '-Os'):
                    hashes = {}
                    for variant, sdk in (('original', baseline_include), ('patched', include)):
                        obj = tmp / profile / f'{compiler_name}-{optimization[1:]}-{variant}.o'
                        subprocess.run([compiler, *base_flags, optimization, *flags, '-I' + str(sdk),
                            '-c', str(ROOT / 'test/contexts/backend_codegen.c'), '-o', str(obj)], check=True)
                        hashes[variant] = digest(obj)
                    assert hashes['original'] == hashes['patched'], (profile, compiler_name, optimization, hashes)
                    receipt['default_object_parity'].append({'profile': profile, 'compiler': compiler_name,
                        'optimization': optimization, 'sha256': hashes['patched'], 'identical': True})
        (args.output / 'results.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print('Contexts backend liveness checks PASS', flush=True)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('runtime', 'x4-runtime', 'utilities', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--baseline-system', type=Path)
    parser.add_argument('--cc', default=os.environ.get('CC', 'cc'))
    parser.add_argument('--target-cc')
    parser.add_argument('--sanitize', action='store_true')
    main(parser.parse_args())

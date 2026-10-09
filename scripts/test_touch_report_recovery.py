#!/usr/bin/env python3
"""Compile the real touch consumer in native and legacy modes; no device I/O."""
import argparse
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SCENARIOS = ('poll-error', 'queue-gap', 'poll-gap', 'queue-budget',
             'multi-contact', 'bad-coordinate', 'provider-fault', 'unknown-fault',
             'snapshot-fault', 'poll-snapshot-fault')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--include', type=Path, action='append', default=[])
    parser.add_argument('--target-cc', type=Path)
    parser.add_argument('--output', type=Path, default=ROOT/'build/touch-report-recovery')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    include = ['-I'+str(ROOT/'lib/PortableApps/include')]
    include += ['-I'+str(p) for p in args.include]
    results = []
    for native in (False, True):
        for sanitized in (False, True):
            label = ('native' if native else 'legacy') + ('-sanitized' if sanitized else '-normal')
            exe = args.output/label
            flags = ['-DPORTABLE_NATIVE_CUSTODY_FENCE'] if native else []
            flags += ['-DPORTABLE_TOUCH_SCROLL']
            if sanitized:
                flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie']
            subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', *flags, *include, str(ROOT/'tests/touch_report_recovery.c'), '-o', str(exe)], check=True)
            for scenario in SCENARIOS:
                env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0')
                run = subprocess.run([str(exe), scenario], text=True, capture_output=True, env=env)
                print(label, run.stdout.strip(), flush=True)
                if run.returncode:
                    raise RuntimeError(label+' '+scenario+'\n'+run.stdout+run.stderr)
                results.append({'profile': label, 'scenario': scenario, 'passed': True})
    if args.target_cc:
        # Build the same production header as ordinary Xtensa PIC code, with
        # explicit imports. This compile artifact is not a flashable firmware.
        target = args.output/'touch-consumer-target.c'
        target.write_text('#define PORTABLE_NATIVE_CUSTODY_FENCE 1\n#define PORTABLE_TOUCH_SCROLL 1\n#include "PortableTouch.h"\nvoid touch_consumer_read(portable_touch*t,portable_touch_sample*s){portable_touch_read(t,s);}\n')
        subprocess.run([str(args.target_cc), '-std=c11', '-Os', '-fPIC', '-mtext-section-literals', '-mlongcalls', '-ffreestanding', '-fno-builtin', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', *include, '-c', str(target), '-o', str(args.output/'touch-consumer-xtensa.o')], check=True)
    (args.output/'results.json').write_text(json.dumps({'schema': 1, 'runs': len(results), 'results': results, 'hardware_tested': False}, indent=2)+'\n')

if __name__ == '__main__':
    main()

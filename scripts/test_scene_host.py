#!/usr/bin/env python3
"""Compile and exercise the real optional presenter, without hardware."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
from scene_sdk import stage_sdk

ROOT = Path(__file__).resolve().parents[1]
MODES = ('behavior', 'inflight', 'stale-frame', 'gap', 'transient', 'held',
         'busy-acquire', 'status-fault', 'snapshot-fault', 'provider-fault', 'close-fault')


def run(runtime: Path, output: Path, sanitize: bool) -> int:
    include = stage_sdk(runtime, ROOT, output / 'sdk')
    compiler = os.environ.get('CC', 'cc')
    flags = ['-std=c11', '-O1', '-Wall', '-Wextra', '-Werror']
    if sanitize:
        flags += ['-g', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                  '-fno-omit-frame-pointer']
    exe = output / 'scene-host-test'
    subprocess.run([compiler, *flags, '-I'+str(include),
                    str(ROOT/'Services/scene_host/host.c'),
                    str(ROOT/'test/scene/host_test.c'), '-o', str(exe)], check=True)
    env = dict(os.environ)
    env.setdefault('ASAN_OPTIONS', 'detect_leaks=0')
    for profile in ('watch', 'paper', 'gray'):
        for mode in MODES:
            command = [str(exe), mode, profile]
            if mode == 'behavior':
                command += [str(output/f'{profile}.frame')]
            subprocess.run(command, env=env, check=True)
    return len(MODES)*3


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='risc-scene-host-') as temporary:
        count = run(args.runtime.resolve(), Path(temporary), args.sanitize)
    print(f'{count} presenter/profile executions passed; hardware remains untested.')

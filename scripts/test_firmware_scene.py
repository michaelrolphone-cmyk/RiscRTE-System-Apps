#!/usr/bin/env python3
"""Compile the actual shared firmware scene once per mode against exact Runtime headers.
Synthetic provider tests only. No network, target device, flashing, or publishing.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

S = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--sanitize', action='store_true', help='Also run ASan/UBSan (normal always runs)')
    args = parser.parse_args()
    output = args.output.resolve() if args.output else Path(tempfile.mkdtemp(prefix='firmware-scene-'))
    output.mkdir(parents=True, exist_ok=True)
    previous = output / 'receipt.json'
    if previous.exists():
        previous.rename(output / ('receipt-previous-' + hashlib.sha256(previous.read_bytes()).hexdigest()[:12] + '.json'))
    include = output / 'include'
    shutil.copytree(S / 'lib/PortableApps/include', include, dirs_exist_ok=True)
    shutil.copytree(S / 'sdk/app', include, dirs_exist_ok=True)
    runtime = args.runtime.resolve() / 'sdk/app'
    for header in runtime.glob('*.h'):
        shutil.copyfile(header, include / header.name)
    assert (runtime / 'RiscRuntimeV1.h').is_file(), runtime
    source = S / 'test/native_apps/firmware_scene_test.c'
    runs = []
    for sanitized in ([False, True] if args.sanitize else [False]):
        name = 'firmware-scene-sanitized' if sanitized else 'firmware-scene'
        flags = ['cc', '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', '-Wno-misleading-indentation']
        if sanitized:
            flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie']
        command = flags + ['-I'+str(include), '-I'+str(S/'lib/NativeApps/include'), str(source), '-o', str(output/name)]
        build = subprocess.run(command, capture_output=True, text=True)
        (output/(name+'-build.log')).write_text(build.stdout+build.stderr)
        if build.returncode:
            print(build.stdout+build.stderr, end='')
        build.check_returncode()
        run = subprocess.run([str(output/name)], capture_output=True, text=True, timeout=30,
                             env={**os.environ, 'ASAN_OPTIONS':'detect_leaks=0', 'UBSAN_OPTIONS':'halt_on_error=1'})
        (output/(name+'.log')).write_text(run.stdout+run.stderr)
        print(run.stdout+run.stderr, end='', flush=True)
        run.check_returncode()
        runs.append({'sanitized':sanitized, 'passed':True})
    sources = [source, Path(__file__), *[S/'Apps'/name for name in ['ota_update_scene.c', 'files_scene_ui.h', 'files_scene_resident.h', 'files_scene_effects.h', 'ota_update_scene_background.h']]]
    receipt = {'passed':True, 'runs':runs, 'hardware':False, 'scope':'Actual shared controller with deterministic synthetic scene, update, Wi-Fi, credentials, time and host services.',
               'source_sha256':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources},
               'runtime_headers_sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in runtime.glob('*.h')}}
    (output/'receipt.json').write_text(json.dumps(receipt, indent=2)+'\n')
    print('Receipt:', output/'receipt.json')

if __name__ == '__main__':
    main()

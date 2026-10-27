#!/usr/bin/env python3
"""Compile the actual adapter and Settings model with canonical Runtime SDK."""
import argparse, hashlib, json, os, shutil, subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--runtime', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
out = a.output.resolve(); inc = out / 'include'
shutil.copytree(ROOT / 'lib/PortableApps/include', inc, dirs_exist_ok=True)
shutil.copytree(ROOT / 'lib/PortableApps/time', out / 'time', dirs_exist_ok=True)
for header in (a.runtime / 'sdk/app').glob('*.h'):
    shutil.copyfile(header, inc / header.name)
cases = 'wake refused retained failure unknown acquire-retained work audio radio wifi frame service-busy service-retained disabled invalid wrap modal'.split()
cases += 'eligibility-retained unsubscribe-retained snapshot-retained navigation-retained service-success-retained configure-retained modal-retained modal-failure modal-host-retained touch-held configure-unavailable timer-save timer-cancel timer-failed timer-deep'.split()
cases += 'retry-success retry-work retry-frame retry-modal retry-retained retry-unavailable retry-backoff retry-service-busy retry-save'.split()
runs = []
for sanitized in (False, True):
    exe = out / ('sanitized' if sanitized else 'normal')
    flags = ['-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function']
    if sanitized:
        flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie']
    subprocess.run([os.environ.get('CC', 'cc'), *flags, '-I'+str(inc), '-I'+str(ROOT/'lib/NativeApps/include'),
                    str(ROOT/'Apps/settings.c'), str(ROOT/'test/native_apps/foreground_idle_test.c'), '-o', str(exe)], check=True)
    for case in cases:
        run = subprocess.run([exe, case], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                             timeout=15, env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0'))
        (out / f'{exe.name}-{case}.log').write_text(run.stdout)
        print(run.stdout, end='', flush=True); run.check_returncode()
        runs.append({'sanitized': sanitized, 'case': case, 'result': 'pass'})
sources = [ROOT/'lib/PortableApps/src'/name for name in ('adapter.c','foreground_idle.inc','foreground_adapter_open.inc','native_custody_adapter.inc','settings.inc','text_input_policy.inc','text_input_handoff.inc')]
sources += [ROOT/'lib/PortableApps/include/PortableForegroundIdle.h', ROOT/'lib/PortableApps/include/PortableNativeCustody.h',
            ROOT/'test/native_apps/foreground_idle_test.c', Path(__file__)]
(out/'evidence.json').write_text(json.dumps({'hardware_verified': False, 'runs': runs,
    'scope': 'Actual shared adapter and Settings model, synthetic hardware and Light helper; modal policy bridge tested without a text provider.',
    'source_sha256': {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest() for path in sources}}, indent=2)+'\n')

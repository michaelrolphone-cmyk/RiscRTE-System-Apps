#!/usr/bin/env python3
"""Reproduce the X4 controller fixture with current, hash-recorded shared SDK headers."""
import argparse, hashlib, json, os, shutil, subprocess
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--runtime', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=False)
include = a.output / 'include'
shutil.copytree(ROOT / 'lib/PortableApps/include', include)
shutil.copytree(a.runtime / 'sdk/app', include, dirs_exist_ok=True)
shutil.copytree(ROOT / 'lib/PortableApps/time', a.output / 'time')
flags = '''PORTABLE_SETTINGS_ATOMIC_EXPERIMENTAL PORTABLE_SETTINGS_HOME_DESK_LOCK
PORTABLE_RESIDENT_SHELL_CLIENT PORTABLE_RESIDENT_POLICY PORTABLE_TOUCH_SCROLL
PORTABLE_SETTINGS_LIST_SCROLL TEST_NATIVE_SETTINGS_ALARMS'''.split()
# Keep the same cases as the deployed-profile runner without executing its CLI.
import ast
module = ast.parse((ROOT / 'scripts/test_settings_atomic.py').read_text())
assignment = next(n for n in module.body if isinstance(n, ast.Assign) and any(isinstance(t, ast.Name) and t.id == 'CASES' for t in n.targets))
cases = ast.literal_eval(assignment.value.func.value).split()
sources = [ROOT / 'test/native_apps/settings_atomic_workflow_test.c', ROOT / 'Apps/settings_native_entry.c']
sources += [ROOT / 'lib/PortableApps/src' / n for n in ('PortableSetTime.c', 'PortableRealtimeClient.c', 'PortableTimeZone.c', 'PortableTimeZoneCatalog.c', 'PortableTimeZonePreference.c')]
cmd = [os.environ.get('CC', 'cc'), '-std=c11', '-O0', '-g', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie', *['-D'+f for f in flags], '-I'+str(include), '-I'+str(ROOT/'lib/NativeApps/include'), *map(str, sources), '-Wl,--wrap=free', '-o', str(a.output/'settings')]
inputs = sorted(set([*sources, Path(__file__).resolve(), *include.rglob('*.h'), *(ROOT/'lib/PortableApps/src').glob('*'), *(ROOT/'test/native_apps').glob('*.c')]))
hashes = {str(f): hashlib.sha256(f.read_bytes()).hexdigest() for f in inputs if f.is_file()}
subprocess.run(cmd, check=True)
for case in cases:
    subprocess.run([str(a.output/'settings'), case], check=True, timeout=20, env={**os.environ, 'ASAN_OPTIONS':'detect_leaks=0', 'UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'})
if any(hashlib.sha256(Path(f).read_bytes()).hexdigest()!=h for f,h in hashes.items()):
    raise SystemExit('Source changed during qualification')
(a.output/'receipt.json').write_text(json.dumps(dict(cases=cases, command=cmd, input_sha256=hashes, hardware_tested=False, scope='Actual shared Settings controller with X4 list-scroll profile and scheduled RTC/native/atomic provider doubles'), indent=2)+'\n')
print(f'{len(cases)} current-source X4 Settings cases PASS (ASan/UBSan); hardware not tested.')

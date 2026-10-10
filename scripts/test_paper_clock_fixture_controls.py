#!/usr/bin/env python3
"""Source-bound positive and negative controls for the Clock provider repair."""
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
out = ROOT / 'build/paper-clock/fixture-controls'
out.mkdir(parents=True, exist_ok=True)
flags = ['-DTEST_NATIVE_LANDSCAPE', '-DPORTABLE_DISPLAY_ROTATION=90',
         '-DPORTABLE_APP_OWNS_TOUCH_CHROME', '-DPORTABLE_RTC_WALL_TIME',
         '-DPORTABLE_ALARM_CLIENT']
app = ROOT / 'Apps/paper_clock.c'
adapter = ROOT / 'lib/PortableApps/src/adapter.c'
fixture = ROOT / 'test/native_apps/paper_clock_test.c'
inputs = [app, adapter, fixture, ROOT / 'test/native_apps/paper_clock_ordered_fixture.h',
          ROOT / 'lib/PortableApps/include/PortableTouch.h', Path(__file__).resolve()]

def compile_case(name, source, extra):
    exe = out / name
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
                    *flags, *extra, '-I'+str(ROOT/'Apps'),
                    '-I'+str(ROOT/'lib/PortableApps/include'),
                    '-I'+str(ROOT/'lib/NativeApps/include'),
                    str(source), str(adapter), str(fixture), '-o', str(exe)], check=True)
    return exe

def check(name, exe, scene, success):
    result = subprocess.run([str(exe), str(scene)], capture_output=True, text=True, timeout=15)
    (out / (name+'.log')).write_text(result.stdout + result.stderr)
    assert (result.returncode == 0) == success, (name, result.stdout, result.stderr)
    if not success:
        assert 'launches==1' in result.stderr, (name, result.stderr)
    print(name + ': ' + ('positive pass' if success else 'expected launch assertion detected'))
    return {'case': name, 'scene': scene, 'returncode': result.returncode,
            'expected_success': success}

ordered = compile_case('ordered', app, ['-DTEST_CLOCK_ORDERED_INPUT'])
legacy = compile_case('legacy-empty-next', app, [])
dropped = compile_case('dropped-edges', app,
                       ['-DTEST_CLOCK_ORDERED_INPUT', '-DTEST_CLOCK_DROP_ORDERED_EDGES'])
text = app.read_text()
original = 'abs(c.x-start_x)>=xscale(40)'
assert text.count(original) == 1
mutated = out / 'threshold_mutation.c'
mutated.write_text(text.replace(original, 'abs(c.x-start_x)>=xscale(4000)'))
mutation = compile_case('threshold-mutation', mutated, ['-DTEST_CLOCK_ORDERED_INPUT'])
runs = [check('legacy-empty-next', legacy, 1, False),
        check('ordered', ordered, 1, True),
        check('dropped-edges', dropped, 1, False),
        check('threshold-mutation', mutation, 1, False)]
receipt = {'runs': runs, 'source_sha256': {
    str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
    for path in inputs}, 'production_mutation': 'temporary compiled copy only',
    'hardware_tested': False}
(out / 'evidence.json').write_text(json.dumps(receipt, indent=2)+'\n')

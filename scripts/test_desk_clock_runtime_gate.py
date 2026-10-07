#!/usr/bin/env python3
"""Test desk-clock storage ordering against real Runtime/CpuPort source.

Run with --runtime /path/to/RiscRTE or RISC_RUNTIME_ROOT=/path/to/RiscRTE.
Both normal and ASan+UBSan builds run by default. Host hardware callbacks model
only pin/arm state; Runtime, CpuPort, provider graph, and app grants are real.
This complements the actual Clock rendering/adapter suite; it is not a display
provider, hardware qualification, or fresh-device wake test.
"""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--runtime', type=Path, default=os.environ.get('RISC_RUNTIME_ROOT'))
parser.add_argument('--sanitizer', choices=('both', 'none', 'address', 'undefined'), default='both',
                    help='both runs normal then ASan+UBSan; address runs ASan+UBSan only')
args = parser.parse_args()
if args.runtime is None:
    parser.error('provide --runtime PATH or RISC_RUNTIME_ROOT')
runtime = args.runtime.resolve()
sources = ['src/bootstrap/Json.cpp', 'src/bootstrap/Board.cpp', 'src/bootstrap/Runtime.cpp',
           'src/runtime/drivers/ProviderGraphV2.cpp', 'src/runtime/drivers/ProviderModuleV2.cpp',
           'src/ports/esp32s3/CpuPort.cpp']
for source in sources:
    if not (runtime / source).is_file():
        parser.error(f'missing Runtime source: {runtime / source}')
includes = [runtime / name for name in ('src', 'sdk/app', 'sdk/driver', 'sdk/hardware',
                                       'lib/ArduinoJson/src', 'test/drivers/stubs')]
includes += [ROOT / 'lib/PortableApps/include', ROOT / 'test/native_apps/fixtures']
include_flags = ['-I' + str(path) for path in includes]
cc = shlex.split(os.environ.get('CC', 'cc'))
cxx = shlex.split(os.environ.get('CXX', 'c++'))
modes = ('none', 'address') if args.sanitizer == 'both' else (args.sanitizer,)
for sanitizer in modes:
    with tempfile.TemporaryDirectory(prefix='desk-clock-runtime-gate-') as directory:
        build = Path(directory)
        flags = ['-Wall', '-Wextra', '-Werror']
        if sanitizer != 'none':
            flags += ['-fsanitize=' + ('address,undefined' if sanitizer == 'address' else 'undefined'),
                      '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-g']
        dynamic = ['-undefined', 'dynamic_lookup'] if sys.platform == 'darwin' else []
        for fixture, output in (('app', 'default.elf'), ('provider', 'provider.elf')):
            source = ROOT / f'test/native_apps/fixtures/desk_clock_runtime_gate_{fixture}.c'
            subprocess.run(cc + flags + ['-std=c11', '-fPIC', '-fvisibility=hidden', '-shared'] +
                           dynamic + include_flags + [str(source), '-o', str(build / output)], check=True)
        executable = build / 'test'
        no_pie = ['-no-pie'] if sanitizer == 'address' and sys.platform.startswith('linux') else []
        subprocess.run(cxx + flags + no_pie + ['-std=c++17', '-Wno-missing-field-initializers', '-rdynamic'] +
                       include_flags + [str(runtime / source) for source in sources] +
                       [str(ROOT / 'test/native_apps/desk_clock_runtime_gate_test.cpp'),
                        '-ldl', '-o', str(executable)], check=True)
        print(f'Desk-clock Runtime gate: {sanitizer}', flush=True)
        # LeakSanitizer cannot inspect processes under traced CI/executor runs.
        # Address/undefined-behavior instrumentation remains enabled.
        environment = dict(os.environ)
        environment['ASAN_OPTIONS'] = environment.get('ASAN_OPTIONS', '') + ':detect_leaks=0'
        for case in range(3):
            result = subprocess.run([str(executable), str(build), str(case)], env=environment, timeout=30)
            expected = 73 if case == 2 else 0
            if result.returncode != expected:
                raise SystemExit(f'case {case}: expected exit {expected}, got {result.returncode}')
print('Desk-clock real Runtime/CpuPort gate regression PASS')

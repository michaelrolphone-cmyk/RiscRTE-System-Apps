#!/usr/bin/env python3
"""Exercise copied crash evidence with the real resident host and Runtime/Graph."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
CASES = ('normal', 'load', 'abi', 'init', 'repeat', 'prior', 'prior-empty', 'prior-long', 'prior-token',
         'held', 'touch', 'retained', 'acquire', 'surface', 'submit', 'status',
         'timeout', 'navigation', 'touch-failure')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--runtime', type=Path, required=True)
    p.add_argument('--display-sdk', type=Path, required=True)
    p.add_argument('--output-dir', type=Path, default=ROOT/'build/resident-failure')
    p.add_argument('--normal-only', action='store_true')
    args = p.parse_args()
    out = args.output_dir.resolve()
    inc = out/'include'
    shutil.copytree(ROOT/'lib/PortableApps/include', inc, dirs_exist_ok=True)
    shutil.copytree(ROOT/'lib/PortableApps/time', out/'time', dirs_exist_ok=True)
    for name in ('RiscRuntimeV1.h', 'RiscResidentShellV1.h'):
        shutil.copyfile(args.runtime/'sdk/app'/name, inc/name)
    for name in ('RiscDisplayOutputV1.h', 'RiscDisplayOutputPowerV1.h', 'RiscDisplayOutputMetricsV1.h', 'RiscDisplayOutputSnapshotV1.h'):
        shutil.copyfile(args.display_sdk/name, inc/name)
    if (args.runtime/'sdk/app/RiscFailureEvidenceV1.h').is_file():
        shutil.copyfile(args.runtime/'sdk/app/RiscFailureEvidenceV1.h', inc/'RiscFailureEvidenceV1.h')
    results = []
    for sanitized in ([False] if args.normal_only else [False, True]):
        build = out/('sanitized' if sanitized else 'normal')
        build.mkdir(parents=True, exist_ok=True)
        san = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer'] if sanitized else []
        base = ['-O1', '-g', '-Wall', '-Wextra', '-Werror', *san, '-I'+str(inc), '-I'+str(ROOT/'lib/NativeApps/include')]
        app = ROOT/'test/native_apps/resident_failure_app.c'
        flags = ['-DPORTABLE_PAPER_PREFERENCES', '-DPORTABLE_ALARM_TERMINAL_RETENTION', '-DPORTABLE_NATIVE_CUSTODY_FENCE',
                 '-DPORTABLE_ALARM_CLIENT', '-DPORTABLE_INPUT_NAVIGATION', '-DPORTABLE_APP_OWNS_TOUCH_CHROME',
                 '-DPORTABLE_HOME_APP="default.elf"', '-DPORTABLE_RESIDENT_SHELL_HOST', '-DPORTABLE_RESIDENT_LEGACY_HANDOFF',
                 '-DPORTABLE_QUICK_ACTIONS', '-DPORTABLE_PAPER_TRANSITIONS']
        sources = [app, *(ROOT/'lib/PortableApps/src'/name for name in ('adapter.c', 'quick_actions.c', 'quick_session.c', 'quick_render.c'))]
        subprocess.run(['cc', '-std=c11', *base, '-fPIC', '-shared', '-Wl,-Bsymbolic', *flags, *map(str, sources), '-o', str(build/'host.elf')], check=True)
        for name in ('child', 'bad'):
            subprocess.run(['cc', '-std=c11', *base, '-fPIC', '-shared', *(['-DFAILURE_BAD_ABI'] if name == 'bad' else []), str(app), '-o', str(build/(name+'.elf'))], check=True)
        fixture = ROOT/'test/native_apps/resident_failure_fixture.c'
        for index, (cap, version) in enumerate((('display.output', 1), ('input.touch.raw', 1), ('input.navigation', 1), ('board.battery', 1), ('rtc.clock', 2), ('alarm.service', 1)), 1):
            subprocess.run(['cc', '-std=c11', *base, '-I'+str(args.runtime/'sdk/driver'), '-fPIC', '-shared',
                            '-DFAILURE_PROVIDER='+str(index), '-DFAILURE_CAPABILITY="'+cap+'"', '-DFAILURE_VERSION='+str(version),
                            str(fixture), '-o', str(build/('provider-'+str(index)+'.elf'))], check=True)
        subprocess.run(['cc', '-std=c11', *base, '-c', str(fixture), '-o', str(build/'fixture.o')], check=True)
        rincs = ['-I'+str(args.runtime/path) for path in ('src', 'sdk/app', 'sdk/driver', 'sdk/hardware', 'lib/ArduinoJson/src', 'test/drivers/stubs')]
        rsources = [args.runtime/path for path in ('src/bootstrap/Json.cpp', 'src/bootstrap/Board.cpp', 'src/bootstrap/Runtime.cpp',
                    'src/runtime/streams/AppStreamSessions.cpp', 'src/runtime/streams/ProviderQueueHost.cpp',
                    'src/runtime/drivers/ProviderGraphV2.cpp', 'src/runtime/drivers/ProviderModuleV2.cpp')]
        binary = build/'resident-failure-test'
        subprocess.run(['c++', '-std=c++17', '-g', '-Wall', '-Wextra', '-Werror', '-Wno-missing-field-initializers', *san, *rincs,
                        '-rdynamic', *(['-no-pie'] if sanitized else []), *map(str, rsources), str(ROOT/'test/native_apps/resident_failure_runtime.cpp'),
                        str(build/'fixture.o'), '-ldl', '-o', str(binary)], check=True)
        for case in CASES:
            shutil.copyfile(build/('bad.elf' if case == 'abi' else 'child.elf'), build/'client.elf')
            if case == 'load':
                (build/'client.elf').unlink()
            result = subprocess.run([str(binary), str(build), case], check=True, timeout=30, text=True, stdout=subprocess.PIPE,
                                    env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
            print(('sanitized ' if sanitized else 'normal ')+result.stdout.strip(), flush=True)
            results.append({'case': case, 'sanitized': sanitized, 'result': result.stdout.strip()})
    source_paths = [ROOT/'lib/PortableApps/src'/name for name in ('adapter.c', 'resident_shell.inc', 'resident_failure.inc')]
    source_paths += list((ROOT/'test/native_apps').glob('resident_failure_*'))+[Path(__file__).resolve()]
    (out/'evidence.json').write_text(json.dumps({'runtime_commit': subprocess.check_output(['git', '-C', str(args.runtime), 'rev-parse', 'HEAD'], text=True).strip(),
        'hardware_tested': False, 'runtime_mocked': False, 'runs': results,
        'source_sha256': {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest() for path in source_paths}}, indent=2)+'\n')
    try:
        from PIL import Image
        for path in out.glob('*/failure-*.pbm'):
            Image.open(path).save(path.with_suffix('.png'))
    except ImportError:
        pass


if __name__ == '__main__':
    main()

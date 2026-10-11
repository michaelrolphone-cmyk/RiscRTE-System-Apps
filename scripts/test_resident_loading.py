#!/usr/bin/env python3
"""Deterministic shared-host loading/presentation qualification; no hardware.

Compiles the production resident controller and display adapter. Loading delays,
child frames and providers are synthetic, independently of any launcher fault.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
CASES = ['slow', 'sync', 'chain', 'repeat', 'resume', 'unknown', 'long', 'refused',
         'busy', 'writable-refused', 'load-failed', 'legacy', 'no-pending', 'fenced',
         'retained', 'acquire', 'submit', 'status', 'timeout']


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--runtime-sdk', type=Path, required=True)
    p.add_argument('--display-sdk', type=Path, required=True)
    p.add_argument('--output-dir', type=Path, required=True)
    p.add_argument('--normal-only', action='store_true')
    args = p.parse_args()
    out = args.output_dir.resolve()
    inc = out/'include'
    shutil.copytree(ROOT/'lib/PortableApps/include', inc, dirs_exist_ok=True)
    shutil.copytree(ROOT/'lib/PortableApps/time', out/'time', dirs_exist_ok=True)
    for name in ('RiscRuntimeV1.h', 'RiscResidentShellV1.h', 'RiscFailureEvidenceV1.h'):
        shutil.copyfile(args.runtime_sdk/name, inc/name)
    for name in ('RiscDisplayOutputV1.h', 'RiscDisplayOutputPowerV1.h',
                 'RiscDisplayOutputMetricsV1.h', 'RiscDisplayOutputSnapshotV1.h'):
        shutil.copyfile(args.display_sdk/name, inc/name)
    flags = ['-DPORTABLE_PAPER_PREFERENCES', '-DPORTABLE_ALARM_TERMINAL_RETENTION',
             '-DPORTABLE_NATIVE_CUSTODY_FENCE', '-DPORTABLE_ALARM_CLIENT',
             '-DPORTABLE_INPUT_NAVIGATION', '-DPORTABLE_APP_OWNS_TOUCH_CHROME',
             '-DPORTABLE_HOME_APP="default.elf"', '-DPORTABLE_RESIDENT_SHELL_HOST',
             '-DPORTABLE_RESIDENT_LEGACY_HANDOFF', '-DPORTABLE_RESIDENT_LOADING',
             '-DPORTABLE_QUICK_ACTIONS', '-DPORTABLE_PAPER_TRANSITIONS', '-DPORTABLE_PAPER_CROSSFADE']
    sources = [ROOT/'test/native_apps/resident_loading_app.c',
               *(ROOT/'lib/PortableApps/src'/name for name in
                 ('adapter.c', 'quick_actions.c', 'quick_session.c', 'quick_render.c'))]
    results = []
    for sanitized in ([False] if args.normal_only else [False, True]):
        build = out/('sanitized' if sanitized else 'normal')
        build.mkdir(parents=True, exist_ok=True)
        san = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer'] if sanitized else []
        base = ['cc', '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                *san, '-I'+str(inc), '-I'+str(ROOT/'lib/NativeApps/include')]
        subprocess.run([*base, '-fPIC', '-shared', '-Wl,-Bsymbolic', *flags,
                        *map(str, sources), '-o', str(build/'host.elf')], check=True)
        symbols = subprocess.check_output(['nm', str(build/'host.elf')], text=True)
        assert ' resident_loading' in symbols
        client_flags = [flag for flag in flags if flag not in
                        ('-DPORTABLE_RESIDENT_SHELL_HOST', '-DPORTABLE_RESIDENT_LEGACY_HANDOFF',
                         '-DPORTABLE_RESIDENT_LOADING', '-DPORTABLE_QUICK_ACTIONS',
                         '-DPORTABLE_PAPER_TRANSITIONS', '-DPORTABLE_PAPER_CROSSFADE')]
        subprocess.run([*base, '-fPIC', '-shared', '-Wl,-Bsymbolic', *client_flags,
                        '-DPORTABLE_RESIDENT_SHELL_CLIENT', str(ROOT/'test/native_apps/resident_shell_app.c'),
                        str(ROOT/'lib/PortableApps/src/adapter.c'), '-o', str(build/'client.elf')], check=True)
        client_symbols = subprocess.check_output(['nm', str(build/'client.elf')], text=True)
        assert ' resident_loading' not in client_symbols and ' pqa_render' not in client_symbols
        binary = build/'resident-loading-test'
        subprocess.run([*base, '-rdynamic', *(['-no-pie'] if sanitized else []),
                        str(ROOT/'test/native_apps/resident_loading_test.c'), '-ldl', '-o', str(binary)], check=True)
        for case in CASES:
            result = subprocess.run([str(binary), str(build), case], check=True, timeout=30,
                                    text=True, stdout=subprocess.PIPE,
                                    env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
            print(('sanitized ' if sanitized else 'normal ')+result.stdout.strip(), flush=True)
            results.append({'case': case, 'sanitized': sanitized, 'result': result.stdout.strip()})
    sources += [ROOT/'lib/PortableApps/src'/name for name in
                ('resident_loading.inc', 'resident_shell.inc', 'nova.inc', 'paper.inc', 'paper_transition.inc')]
    sources += [ROOT/'lib/PortableApps/fonts'/name for name in ('icons.inc', 'springboard_gamepad.inc')]
    sources += [ROOT/'test/native_apps/resident_loading_test.c', ROOT/'test/native_apps/resident_shell_test.c',
                ROOT/'test/native_apps/resident_shell_app.c', Path(__file__)]
    if not args.normal_only:
        for image in (out/'normal').glob('loading-*.pbm'):
            assert image.read_bytes() == (out/'sanitized'/image.name).read_bytes(), image.name
    receipt = {'hardware_tested': False, 'runtime_mocked': True,
               'scope': 'Actual shared host controller and adapter; synthetic child load and display providers',
               'client_loading_renderer': 'absent in compiled resident client adapter',
               'normal_sanitized_pixels': 'identical' if not args.normal_only else 'not compared',
               'runs': results, 'process_cases': len(results),
               'sdk_sha256': {name: hashlib.sha256((inc/name).read_bytes()).hexdigest()
                              for name in ('RiscRuntimeV1.h', 'RiscResidentShellV1.h')},
               'source_sha256': {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest() for path in sources}}
    (out/'evidence.json').write_text(json.dumps(receipt, indent=2)+'\n')
    from PIL import Image
    for source in out.glob('*/loading-*.pbm'):
        Image.open(source).save(source.with_suffix('.png'))


if __name__ == '__main__':
    main()

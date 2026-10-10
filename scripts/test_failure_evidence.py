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
CASES = ('normal', 'landscape', 'empty', 'unsupported', 'invalid-stack', 'retention', 'long', 'none',
         'acknowledged', 'old-runtime', 'repeat', 'stale', 'ack-denied', 'held', 'touch', 'back', 'home', 'right', 'acquire', 'surface', 'submit', 'status', 'failed', 'superseded',
         'timeout', 'navigation', 'touch-failure')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--runtime', type=Path, required=True)
    p.add_argument('--display-sdk', type=Path, required=True)
    p.add_argument('--output-dir', type=Path, default=ROOT/'build/failure-evidence')
    p.add_argument('--old-system', type=Path, help='Frozen pre-evidence System checkout for prefix compatibility')
    p.add_argument('--normal-only', action='store_true')
    args = p.parse_args()
    out = args.output_dir.resolve()
    inc = out/'include'
    shutil.copytree(ROOT/'lib/PortableApps/include', inc, dirs_exist_ok=True)
    shutil.copytree(ROOT/'lib/PortableApps/time', out/'time', dirs_exist_ok=True)
    for name in ('RiscRuntimeV1.h', 'RiscResidentShellV1.h', 'RiscFailureEvidenceV1.h'):
        shutil.copyfile(args.runtime/'sdk/app'/name, inc/name)
    for name in ('RiscDisplayOutputV1.h', 'RiscDisplayOutputPowerV1.h', 'RiscDisplayOutputMetricsV1.h', 'RiscDisplayOutputSnapshotV1.h'):
        shutil.copyfile(args.display_sdk/name, inc/name)
    results = []
    for sanitized in ([False] if args.normal_only else [False, True]):
        build = out/('sanitized' if sanitized else 'normal')
        build.mkdir(parents=True, exist_ok=True)
        san = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer'] if sanitized else []
        base = ['-O1', '-g', '-Wall', '-Wextra', '-Werror', *san, '-I'+str(inc), '-I'+str(ROOT/'lib/NativeApps/include')]
        app = ROOT/'test/native_apps/failure_evidence_app.c'
        flags = ['-DPORTABLE_DESK_LOCK_HOME', '-DPORTABLE_PAPER_PREFERENCES', '-DPORTABLE_ALARM_TERMINAL_RETENTION', '-DPORTABLE_NATIVE_CUSTODY_FENCE',
                 '-DPORTABLE_ALARM_CLIENT', '-DPORTABLE_INPUT_NAVIGATION', '-DPORTABLE_APP_OWNS_TOUCH_CHROME',
                 '-DPORTABLE_HOME_APP="default.elf"', '-DPORTABLE_RESIDENT_SHELL_HOST', '-DPORTABLE_RESIDENT_LEGACY_HANDOFF',
                 '-DPORTABLE_QUICK_ACTIONS', '-DPORTABLE_PAPER_TRANSITIONS']
        sources = [app, *(ROOT/'lib/PortableApps/src'/name for name in ('adapter.c', 'quick_actions.c', 'quick_session.c', 'quick_render.c'))]
        subprocess.run(['cc', '-std=c11', *base, '-fPIC', '-shared', '-Wl,-Bsymbolic', '-Drisc_runtime_get_api=failure_runtime_get_api', *flags, *map(str, sources), '-o', str(build/'host.elf')], check=True)
        for name in ('child',):
            subprocess.run(['cc', '-std=c11', *base, '-fPIC', '-shared', *(['-DFAILURE_BAD_ABI'] if name == 'bad' else []), str(app), '-o', str(build/(name+'.elf'))], check=True)
        fixture = ROOT/'test/native_apps/failure_evidence_fixture.c'
        for index, (cap, version) in enumerate((('display.output', 1), ('input.touch.raw', 1), ('input.navigation', 1), ('board.battery', 1), ('rtc.clock', 2), ('alarm.service', 1)), 1):
            subprocess.run(['cc', '-std=c11', *base, '-I'+str(args.runtime/'sdk/driver'), '-fPIC', '-shared',
                            '-DFAILURE_PROVIDER='+str(index), '-DFAILURE_CAPABILITY="'+cap+'"', '-DFAILURE_VERSION='+str(version),
                            str(fixture), '-o', str(build/('provider-'+str(index)+'.elf'))], check=True)
        subprocess.run(['cc', '-std=c11', *base, '-c', str(fixture), '-o', str(build/'fixture.o')], check=True)
        rincs = ['-I'+str(args.runtime/path) for path in ('src', 'sdk/app', 'sdk/driver', 'sdk/hardware', 'lib/ArduinoJson/src', 'test/drivers/stubs')]
        rsources = [args.runtime/path for path in ('src/bootstrap/Json.cpp', 'src/bootstrap/Board.cpp', 'src/bootstrap/Runtime.cpp',
                    'src/runtime/streams/AppStreamSessions.cpp', 'src/runtime/streams/ProviderQueueHost.cpp',
                    'src/runtime/drivers/ProviderGraphV2.cpp', 'src/runtime/drivers/ProviderModuleV2.cpp')]
        binary = build/'failure-evidence-test'
        subprocess.run(['c++', '-std=c++17', '-g', '-Wall', '-Wextra', '-Werror', '-Wno-missing-field-initializers', *san, *rincs,
                        '-rdynamic', *(['-no-pie'] if sanitized else []), *map(str, rsources), str(ROOT/'test/native_apps/failure_evidence_runtime.cpp'),
                        str(build/'fixture.o'), '-ldl', '-o', str(binary)], check=True)
        def run(case):
            shutil.copyfile(build/'child.elf', build/'client.elf')
            result = subprocess.run([str(binary), str(build), case], check=True, timeout=30, text=True, stdout=subprocess.PIPE,
                                    env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
            print(('sanitized ' if sanitized else 'normal ')+result.stdout.strip(), flush=True)
            results.append({'case': case, 'sanitized': sanitized, 'result': result.stdout.strip()})
        for case in CASES:
            run(case)
        if args.old_system:
            old_sources = [app, *(args.old_system/'lib/PortableApps/src'/name for name in ('adapter.c', 'quick_actions.c', 'quick_session.c', 'quick_render.c'))]
            old_inc=build/'old-include'
            shutil.copytree(inc,old_inc,dirs_exist_ok=True)
            old_runtime=args.runtime/'test/fixtures/failure_evidence_runtime_0192.h'
            shutil.copyfile(old_runtime,old_inc/'RiscRuntimeV1.h')
            objects=[]
            for index,source in enumerate(old_sources):
                obj=build/('old-source-'+str(index)+'.o');objects.append(obj)
                includes=[] if index==0 else ['-I'+str(old_inc)]
                subprocess.run(['cc','-std=c11',*includes,*base,'-fPIC','-Drisc_runtime_get_api=failure_runtime_get_api',*flags,'-c',str(source),'-o',str(obj)],check=True)
            subprocess.run(['cc',*san,'-shared','-Wl,-Bsymbolic',*map(str,objects),'-o',str(build/'host.elf')],check=True)
            run('old-system')
    source_paths = [ROOT/'lib/PortableApps/src'/name for name in ('adapter.c', 'resident_shell.inc', 'resident_failure.inc')]
    source_paths += [ROOT/'lib/PortableApps/src/failure_evidence.inc']
    source_paths += list((ROOT/'test/native_apps').glob('failure_evidence_*'))+[Path(__file__).resolve()]
    pixels={str(path.relative_to(out)):hashlib.sha256(path.read_bytes()).hexdigest() for path in out.glob('*/failure-*.pbm')}
    if not args.normal_only:
        for path in (out/'normal').glob('failure-*.pbm'):
            assert path.read_bytes()==(out/'sanitized'/path.name).read_bytes(), 'Sanitizer pixels differ: '+path.name
    (out/'evidence.json').write_text(json.dumps({'runtime_commit': subprocess.check_output(['git', '-C', str(args.runtime), 'rev-parse', 'HEAD'], text=True).strip(),
        'runtime_dirty':bool(subprocess.check_output(['git','-C',str(args.runtime),'status','--porcelain'],text=True)),
        'runtime_compiled_source_sha256':{str(path.relative_to(args.runtime)):hashlib.sha256(path.read_bytes()).hexdigest() for path in rsources},
        'old_system_commit':subprocess.check_output(['git','-C',str(args.old_system),'rev-parse','HEAD'],text=True).strip() if args.old_system else None,
        'sdk_sha256':{name:hashlib.sha256((inc/name).read_bytes()).hexdigest() for name in ('RiscRuntimeV1.h','RiscResidentShellV1.h','RiscFailureEvidenceV1.h')},
        'completed_frame_sha256':pixels,'normal_sanitizer_pixels_identical':not args.normal_only,
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

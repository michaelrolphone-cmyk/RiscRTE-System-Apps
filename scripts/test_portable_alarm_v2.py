#!/usr/bin/env python3
"""Production alarm API2 client/adapter gates; provider doubles, no hardware."""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[1]
RUNTIME_REF = '30dcec5ce6ce33223f2b203a2399283e1f758567'
BASE_REF = 'b05d3f01a776ab2708832054eb7dc3c4c04da48a'
FLAGS = ['-DALARM_SERVICE_TAGGED_V2', '-DPORTABLE_NATIVE_CUSTODY_FENCE']
QUICK = ['quick_actions.c', 'quick_render.c', 'quick_session.c']
CASES = ['storage', 'rtc', 'visual', 'sound', 'step-retained', 'status-retained',
         'copied-retained', 'quick-retained', 'ack-retained', 'stop-retained']

def run(command, **kwargs):
    return subprocess.run(list(map(str, command)), check=True, **kwargs)

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--utilities', type=Path, required=True)
    parser.add_argument('--xtensa-cc', type=Path, default=ROOT.parent / 'watch-build-tools/platformio-core/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc')
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'build/portable-alarm-v2')
    args = parser.parse_args()
    sdk = args.runtime.resolve() / 'sdk/app'
    utilities = args.utilities.resolve() / 'lib/Alarm/include'
    out = args.output_dir.resolve(); out.mkdir(parents=True, exist_ok=True)
    cc = os.environ.get('CC', 'cc')
    receipt = {'runtime_ref': RUNTIME_REF, 'baseline_ref': BASE_REF, 'production_headers': {},
               'host_cases': {}, 'flag_off_target_sha256': {}, 'hardware': 'not run'}
    for name in ['RiscRuntimeV1.h', 'RiscRealtimeV1.h']:
        expected = subprocess.check_output(['git', '-C', args.runtime, 'show', RUNTIME_REF + ':sdk/app/' + name])
        assert expected == (sdk / name).read_bytes(), name
        receipt['production_headers'][name] = sha(sdk / name)
    for name in ['AlarmServiceV1.h', 'AlarmServiceV2.h']:
        receipt['production_headers'][name] = sha(utilities / name)
    with tempfile.TemporaryDirectory(prefix='portable-alarm-v2-') as temporary:
        stage = Path(temporary)
        include = stage / 'include'
        shutil.copytree(ROOT / 'lib/PortableApps/include', include)
        shutil.copytree(ROOT / 'lib/PortableApps/time', stage / 'time')
        for name in ['RiscRuntimeV1.h', 'RiscRealtimeV1.h']:
            shutil.copyfile(sdk / name, include / name)
        includes = ['-I' + str(include), '-I' + str(utilities), '-I' + str(ROOT / 'lib/NativeApps/include')]
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        for sanitized in [False, True]:
            label = 'asan-ubsan' if sanitized else 'normal'
            common = [cc, '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', *FLAGS, *includes]
            if sanitized:
                common += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie']
            client = out / ('client-' + label)
            run([*common, ROOT / 'test/native_apps/portable_alarm_v2_test.c', '-o', client])
            run([client], env=env, timeout=20)
            adapter = out / ('adapter-' + label)
            run([*common, '-DPORTABLE_NATIVE_TIME_TOOLBAR', ROOT / 'test/native_apps/portable_alarm_v2_adapter_test.c',
                 *[ROOT / 'lib/PortableApps/src' / name for name in QUICK], '-Wl,--wrap=free', '-o', adapter])
            with (out / (label + '.log')).open('w') as log:
                for case in CASES:
                    result = run([adapter, case], env=env, timeout=20, text=True, stdout=subprocess.PIPE)
                    log.write(result.stdout)
            receipt['host_cases'][label] = ['client descriptor/dispatch matrix', *CASES]
            print('Production alarm API2 ' + label + ': client matrix and 10 adapter cases passed', flush=True)
        # Exact flag-off target comparison against the unchanged System base.
        baseline = stage / 'baseline'; baseline.mkdir()
        archive = subprocess.check_output(['git', '-C', ROOT, 'archive', BASE_REF])
        with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
            tar.extractall(baseline, filter='data')
        baseline_include = stage / 'baseline-stage/include'
        shutil.copytree(baseline / 'lib/PortableApps/include', baseline_include)
        shutil.copytree(baseline / 'lib/PortableApps/time', baseline_include.parent / 'time')
        for name in ['RiscRuntimeV1.h', 'RiscRealtimeV1.h']:
            shutil.copyfile(sdk / name, baseline_include / name)
        target_flags = ['-std=c11', '-Os', '-fPIC', '-mtext-section-literals', '-mlongcalls',
                        '-fvisibility=hidden', '-ffreestanding', '-fno-builtin', '-Wall', '-Wextra', '-Werror',
                        '-DPORTABLE_NATIVE_TIME_TOOLBAR', '-DPORTABLE_NATIVE_CUSTODY_FENCE', '-DPORTABLE_ALARM_CLIENT']
        version = subprocess.check_output([args.xtensa_cc, '--version'], text=True).splitlines()[0]
        assert '8.4.0' in version and '2021r2-patch5' in version, version
        receipt['target_compiler'] = version
        harness = stage / 'harness.c'
        harness.write_text('''#include "PortableApps.h"
#include "PortableNativeTimeToolbar.h"
const t5_app_manifest_t portable_catalog[1]={{.compatible=false}};
const unsigned portable_catalog_count=0;
bool portable_app_native_local_time(twatch_rtc_time_v1 *out){*out=(twatch_rtc_time_v1){2026,10,7,3,13,42,56};return true;}
__attribute__((visibility("default"))) void app_main(void){}
''')
        export_map = stage / 'exports.map'
        export_map.write_text('{ global: app_main; app_module_init; app_module_fini; local: *; };\n')
        for quick in [False, True]:
            profile = 'quick' if quick else 'alarm'
            profile_flags = ['-DPORTABLE_QUICK_ACTIONS'] if quick else []
            outputs = {}
            for name, repo, headers, flags in [('base', baseline, baseline_include, []),
                                              ('off', ROOT, include, []), ('v2', ROOT, include, ['-DALARM_SERVICE_TAGGED_V2'])]:
                dest = out / (profile + '-' + name); dest.mkdir(exist_ok=True)
                target_includes = ['-I' + str(headers), '-I' + str(utilities), '-I' + str(repo / 'lib/NativeApps/include')]
                target_common = [args.xtensa_cc, *target_flags, *profile_flags, *flags, *target_includes]
                run([*target_common, '-c', repo / 'lib/PortableApps/src/adapter.c', '-o', dest / 'adapter.o'])
                helpers = [repo / 'lib/PortableApps/src' / part for part in QUICK] if quick else []
                run([*target_common, '-nostdlib', '-nostartfiles', '-shared', '-Wl,--hash-style=sysv',
                     '-Wl,--version-script=' + str(export_map), dest / 'adapter.o', harness, *helpers, '-lgcc', '-o', dest / 'harness.elf'])
                outputs[name] = dest
            for filename in ['adapter.o', 'harness.elf']:
                assert (outputs['base'] / filename).read_bytes() == (outputs['off'] / filename).read_bytes(), (profile, filename)
                receipt['flag_off_target_sha256'][profile + '/' + filename] = sha(outputs['off'] / filename)
            print('API2 Xtensa ' + profile + ' links; flag-off object/full harness ELF equal baseline', flush=True)
        receipt['target_profiles'] = ['alarm', 'quick']
    (out / 'evidence.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print('Evidence: ' + str(out / 'evidence.json'))

if __name__ == '__main__':
    main()

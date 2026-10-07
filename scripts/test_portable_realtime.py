#!/usr/bin/env python3
"""Production app-client fixtures and optional pinned Xtensa link/loader proof.

Read the exact public Runtime SDK from local Git objects; never fetch or vendor
an independent copy. No app/product is activated and temporary output is bounded.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
from native_app_symbols import firmware_exports, validate_imports

ROOT = Path(__file__).resolve().parents[1]
INCLUDE = ROOT / 'lib/PortableApps/include'
RUNTIME_COMMIT = '341e6e38ce00b7c57d5daaf5f8c829c3575db931'
HEADER_SHA256 = '639781f728841cda6d40c3033877436b9e5d59519a4b15e11a9bc99b2365395d'
SOURCES = [ROOT / 'lib/PortableApps/src' / name for name in (
    'PortableRealtimeClient.c', 'PortableTimeZone.c', 'PortableTimeZoneCatalog.c')]
TRACKED = [*SOURCES, INCLUDE / 'PortableRealtimeClient.h', INCLUDE / 'PortableRtcBasis.h',
           INCLUDE / 'PortableRtcClock.h', INCLUDE / 'PortableTimeZone.h',
           ROOT / 'test/native_apps/portable_realtime_client_test.c',
           ROOT / 'test/native_apps/fixtures/portable_realtime_elf.c', Path(__file__).resolve()]


def run(args, **kwargs):
    try:
        return subprocess.run(list(map(str, args)), check=True, **kwargs)
    except subprocess.CalledProcessError as error:
        if error.stdout:
            print(error.stdout, end="")
        if error.stderr:
            print(error.stderr, end="")
        raise


def xtensa(cc, directory, native_exports):
    version = subprocess.check_output([cc, '--version'], text=True).splitlines()[0]
    assert '8.4.0' in version and '2021r2-patch5' in version, version
    readelf, strip = cc.replace('gcc', 'readelf'), cc.replace('gcc', 'strip')
    flags = [cc, '-std=c11', '-Os', '-fPIC', '-mtext-section-literals', '-mlongcalls',
             '-fvisibility=hidden', '-Wall', '-Wextra', '-Werror',
             '-I' + str(INCLUDE), '-I' + str(directory)]
    for source in SOURCES:
        run([*flags, '-c', source, '-o', directory / (source.stem + '.o')])
    elf = directory / 'portable-realtime.elf'
    run([*flags, '-nostdlib', '-nostartfiles', '-shared', *SOURCES,
         ROOT / 'test/native_apps/fixtures/portable_realtime_elf.c',
         '-Wl,--hash-style=sysv', '-o', elf])
    run([strip, '--strip-unneeded', elf])
    symbols = subprocess.check_output([readelf, '--dyn-syms', '--wide', str(elf)], text=True)
    imports = validate_imports(symbols, firmware_exports(ROOT) | native_exports)
    assert imports <= {'risc_runtime_get_api', 'memcpy', 'memset'}, imports
    assert 'risc_runtime_get_api' in imports
    assert any('GLOBAL' in line and 'FUNC' in line and line.split()[-1] == 'app_main'
               for line in symbols.splitlines())
    validator = directory / 'validate'
    run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
         '-I' + str(ROOT / 'test/native_apps/stubs'), '-I' + str(ROOT / 'lib/elf_loader/include'),
         ROOT / 'lib/elf_loader/src/esp_elf_validate.c',
         ROOT / 'test/native_apps/validate_test.c', '-o', validator])
    run([validator, elf])
    result = {'compiler': version, 'compiler_sha256': hashlib.sha256(Path(cc).read_bytes()).hexdigest(),
              'elf_bytes': elf.stat().st_size, 'elf_sha256': hashlib.sha256(elf.read_bytes()).hexdigest(),
              'imports': sorted(imports), 'loader_validation': 'PASS', 'dynamic_symbols': symbols}
    print('Xtensa pinned object/ELF proof:', result['elf_bytes'], 'bytes; imports:', ', '.join(sorted(imports)))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime-repo', required=True, type=Path,
                        help='Local Runtime Git checkout containing the public pinned SDK commit')
    parser.add_argument('--xtensa-cc', help='Pinned 8.4.0+2021r2-patch5 gcc, no implicit substitution')
    parser.add_argument('--evidence', type=Path, help='Optional source-hashed JSON receipt, not product output')
    args = parser.parse_args()
    header = subprocess.check_output(['git', '-C', str(args.runtime_repo), 'show',
                                     RUNTIME_COMMIT + ':sdk/app/RiscRealtimeV1.h'])
    assert hashlib.sha256(header).hexdigest() == HEADER_SHA256, 'Unexpected Runtime SDK bytes'
    runtime_source = subprocess.check_output(['git', '-C', str(args.runtime_repo), 'show',
                                              RUNTIME_COMMIT + ':src/bootstrap/Runtime.cpp'])
    # Legacy Reader exports omit this portable Runtime entrypoint. Verify its
    # real public registration in the exact pinned Runtime before allowing it.
    registration = b'{"risc_runtime_get_api",reinterpret_cast<const void*>(&risc_runtime_get_api)}'
    assert registration in runtime_source, 'Runtime entrypoint export is missing'
    evidence = {'runtime_export_source_sha256': hashlib.sha256(runtime_source).hexdigest(),
                'runtime_commit': RUNTIME_COMMIT, 'sdk_header_sha256': HEADER_SHA256,
                'source_sha256': {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in TRACKED}}
    with tempfile.TemporaryDirectory(prefix='portable-realtime-') as temp:
        directory = Path(temp)
        (directory / 'RiscRealtimeV1.h').write_bytes(header)
        for sanitized in (False, True):
            executable = directory / ('sanitized' if sanitized else 'normal')
            flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                     '-fno-omit-frame-pointer', '-no-pie'] if sanitized else []
            run([os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-Wall', '-Wextra', '-Werror', '-pedantic',
                 *flags, '-I' + str(INCLUDE), '-I' + str(directory), *SOURCES,
                 ROOT / 'test/native_apps/portable_realtime_client_test.c', '-o', executable])
            completed = run([executable], env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0'),
                            timeout=30, text=True, capture_output=True)
            print(completed.stdout, end='')
            evidence['host_asan_ubsan' if sanitized else 'host_plain'] = completed.stdout.strip()
        evidence['xtensa'] = xtensa(args.xtensa_cc, directory, {'risc_runtime_get_api'}) if args.xtensa_cc else 'NOT RUN (supply --xtensa-cc)'
    if args.evidence:
        args.evidence.parent.mkdir(parents=True, exist_ok=True)
        args.evidence.write_text(json.dumps(evidence, indent=2) + '\n')


if __name__ == '__main__':
    main()

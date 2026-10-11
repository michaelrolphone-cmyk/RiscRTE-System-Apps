#!/usr/bin/env python3
"""Production checked Set Time + all writable-year DST intervals, sanitizer/ELF proof."""
import argparse
from datetime import timedelta
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import test_portable_realtime as realtime
from test_portable_timezone import oracle_rule, event, expected, EPOCH

ROOT = Path(__file__).resolve().parents[1]
SOURCES = [*realtime.SOURCES, ROOT / 'lib/PortableApps/src/PortableSetTime.c']
TEST = ROOT / 'test/native_apps/portable_set_time_test.c'
FIXTURE = ROOT / 'test/native_apps/fixtures/portable_set_time_elf.c'
TRACKED = [*realtime.TRACKED, SOURCES[-1], ROOT / 'lib/PortableApps/include/PortableSetTime.h',
           TEST, FIXTURE, Path(__file__).resolve(), ROOT / 'scripts/test_portable_timezone.py']


def vectors(path):
    catalog = (ROOT / 'lib/PortableApps/src/PortableTimeZoneCatalog.c').read_text()
    rows = re.findall(r'\{"([^"]+)", "([^"]+)", PORTABLE_TIMEZONE_\w+\}', catalog)
    output = ['struct set_vector { unsigned zone; portable_timezone_civil civil; int choice,utc,status; int64_t epoch; };',
              'static const struct set_vector set_vectors[]={']
    count = 0
    for index, (zone, text) in enumerate(rows):
        parsed = oracle_rule(text)
        if not parsed[2]:
            continue
        std, dst = parsed[:2]
        for year in range(2000, 2039):
            for transition, before in ((parsed[2], std), (parsed[3], dst)):
                at = event(year, transition, before)
                lower, upper = sorted((at + std, at + dst))
                # Every gap/fold, first/last affected second and midpoint, with
                # immediate neighbours. Independent datetime and regex oracle.
                for wall_epoch in sorted({lower - 1, lower, (lower + upper) // 2, upper - 1, upper}):
                    wall = EPOCH + timedelta(seconds=wall_epoch)
                    candidates = sorted({wall_epoch - off for off in (std, dst)
                                         if expected(parsed, wall_epoch - off)[0] == wall})
                    for choice in (-1, 0, 1):
                        for utc in (0, 1):
                            epoch = 0
                            if not candidates:
                                status = -10
                            elif len(candidates) == 2 and choice == -1:
                                status = -9
                            else:
                                epoch = candidates[choice if len(candidates) == 2 else 0]
                                calendar = EPOCH + timedelta(seconds=epoch) if utc else wall
                                status = 0 if 946684800 <= epoch <= 2147483647 and 2000 <= calendar.year <= 2099 else -11
                            fields = f'{wall.year},{wall.month},{wall.day},{wall.hour},{wall.minute},{wall.second},255'
                            output.append(f'{{{index},{{{fields}}},{choice},{utc},{status},INT64_C({epoch})}},')
                            count += 1
    output.append('};')
    path.write_text('\n'.join(output) + '\n')
    return count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime-repo', required=True, type=Path)
    parser.add_argument('--xtensa-cc')
    parser.add_argument('--evidence', type=Path)
    args = parser.parse_args()
    header = subprocess.check_output(['git', '-C', str(args.runtime_repo), 'show',
                                     realtime.RUNTIME_COMMIT + ':sdk/app/RiscRealtimeV1.h'])
    assert hashlib.sha256(header).hexdigest() == realtime.HEADER_SHA256
    runtime = subprocess.check_output(['git', '-C', str(args.runtime_repo), 'show',
                                      realtime.RUNTIME_COMMIT + ':src/bootstrap/Runtime.cpp'])
    assert b'{"risc_runtime_get_api",reinterpret_cast<const void*>(&risc_runtime_get_api)}' in runtime
    evidence = {'runtime_commit': realtime.RUNTIME_COMMIT, 'sdk_header_sha256': realtime.HEADER_SHA256,
                'runtime_export_source_sha256': hashlib.sha256(runtime).hexdigest(),
                'source_sha256': {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in TRACKED}}
    with tempfile.TemporaryDirectory(prefix='portable-set-time-') as temp:
        directory = Path(temp)
        (directory / 'RiscRealtimeV1.h').write_bytes(header)
        evidence['independent_catalog_vectors'] = vectors(directory / 'set_time_vectors.h')
        evidence['vectors_sha256'] = hashlib.sha256((directory / 'set_time_vectors.h').read_bytes()).hexdigest()
        for sanitized in (False, True):
            executable = directory / ('sanitized' if sanitized else 'normal')
            flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                     '-fno-omit-frame-pointer', '-no-pie'] if sanitized else []
            realtime.run([os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-Wall', '-Wextra', '-Werror', '-pedantic',
                          *flags, '-I' + str(realtime.INCLUDE), '-I' + str(directory), *SOURCES, TEST, '-o', executable])
            result = realtime.run([executable], env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0'),
                                  timeout=120, text=True, capture_output=True)
            print(result.stdout, end='')
            evidence['host_asan_ubsan' if sanitized else 'host_plain'] = result.stdout.strip()
        realtime.SOURCES = SOURCES
        evidence['xtensa'] = realtime.xtensa(args.xtensa_cc, directory, {'risc_runtime_get_api'}, FIXTURE) if args.xtensa_cc else 'NOT RUN'
    if args.evidence:
        args.evidence.parent.mkdir(parents=True, exist_ok=True)
        args.evidence.write_text(json.dumps(evidence, indent=2) + '\n')


if __name__ == '__main__':
    main()

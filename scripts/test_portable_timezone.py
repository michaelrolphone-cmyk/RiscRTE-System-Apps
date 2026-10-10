#!/usr/bin/env python3
"""Pure timezone host/sanitizer, independent calendar oracle and Xtensa ELF proof.

Only a link harness is built; no Clock/Settings/default/product is activated.
"""
import argparse
import calendar
import ctypes as C
from datetime import datetime, timedelta, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
from native_app_symbols import firmware_exports, validate_imports

ROOT = Path(__file__).resolve().parents[1]
SOURCES = [ROOT / 'lib/PortableApps/src' / name for name in (
    'PortableTimeZone.c', 'PortableTimeZoneCatalog.c', 'PortableTimeZonePreference.c')]
INCLUDE = ROOT / 'lib/PortableApps/include'
UTC = timezone.utc
EPOCH = datetime(1970, 1, 1, tzinfo=UTC)


def run(args, **kw):
    return subprocess.run(list(map(str, args)), check=True, **kw)


class Civil(C.Structure):
    _fields_ = [('year', C.c_int32)] + [(n, C.c_uint8) for n in ('month', 'day', 'hour', 'minute', 'second', 'weekday')]


class Transition(C.Structure):
    _fields_ = [(n, C.c_uint8) for n in ('month', 'week', 'weekday')] + [('seconds', C.c_int32)]


class Rule(C.Structure):
    _fields_ = [('standard_offset', C.c_int32), ('daylight_offset', C.c_int32),
                ('has_daylight', C.c_uint8), ('start', Transition), ('end', Transition)]


class Candidate(C.Structure):
    _fields_ = [('epoch', C.c_int64), ('offset', C.c_int32), ('daylight', C.c_uint8)]


class Inverse(C.Structure):
    _fields_ = [('count', C.c_uint), ('candidate', Candidate * 2)]


def seconds(value):
    sign = -1 if value.startswith('-') else 1
    fields = list(map(int, value.lstrip('+-').split(':')))
    return sign * sum(v * m for v, m in zip(fields, (3600, 60, 1)))


def oracle_rule(text):
    # Independent Python regex/parser, Gregorian datetime and calendar.monthrange.
    name = r'(?:[A-Za-z]{3,16}|<[A-Za-z0-9+:\-]{2,16}>)'
    offset = r'[+\-]?\d{1,3}(?::\d{1,2})?(?::\d{1,2})?'
    date = r'M\d{1,2}\.\d\.\d(?:/' + offset + r')?'
    m = re.fullmatch(rf'{name}({offset})(?:{name}({offset})?,({date}),({date}))?', text)
    assert m, text
    std = -seconds(m[1])
    return std, (-seconds(m[2]) if m[2] else std + 3600) if m[3] else std, m[3], m[4]


def event(year, text, before):
    date, _, at = text.partition('/')
    month, week, dow = map(int, date[1:].split('.'))
    days = [day for day in range(1, calendar.monthrange(year, month)[1] + 1)
            if (datetime(year, month, day).weekday() + 1) % 7 == dow]
    day = days[-1] if week == 5 else days[week - 1]
    when = datetime(year, month, day, tzinfo=UTC) + timedelta(seconds=seconds(at) if at else 7200)
    return int((when - EPOCH).total_seconds()) - before


def expected(parsed, epoch):
    std, dst, start, end = parsed
    utc = EPOCH + timedelta(seconds=epoch)
    active = False
    if start:
        events = [(event(y, start, std), True) for y in range(utc.year - 1, utc.year + 2)]
        events += [(event(y, end, dst), False) for y in range(utc.year - 1, utc.year + 2)]
        active = max((when, flag) for when, flag in events if when <= epoch)[1]
    offset = dst if active else std
    return utc + timedelta(seconds=offset), offset, active


def oracle(shared):
    lib = C.CDLL(str(shared))
    lib.portable_timezone_parse.argtypes = [C.c_char_p, C.c_size_t, C.POINTER(Rule)]
    lib.portable_timezone_utc_to_local.argtypes = [C.POINTER(Rule), C.c_int64, C.POINTER(Civil), C.POINTER(Candidate)]
    lib.portable_timezone_local_to_utc.argtypes = [C.POINTER(Rule), C.POINTER(Civil), C.POINTER(Inverse)]
    catalog = (ROOT / 'lib/PortableApps/src/PortableTimeZoneCatalog.c').read_text()
    rows = re.findall(r'\{"([^"]+)", "([^"]+)", PORTABLE_TIMEZONE_\w+\}', catalog)
    assert len(rows) == 419
    vectors = 0
    for zone, text in rows:
        parsed = oracle_rule(text)
        rule = Rule()
        assert lib.portable_timezone_parse(text.encode(), len(text) + 1, C.byref(rule)) == 0
        for year in (1900, 1999, 2000, 2024, 2026, 2038, 2100, 2400, 9998):
            epochs = {int((datetime(year, m, 1, tzinfo=UTC) - EPOCH).total_seconds()) for m in (1, 7, 12)}
            if parsed[2]:
                for transition, before in ((parsed[2], parsed[0]), (parsed[3], parsed[1])):
                    at = event(year, transition, before)
                    epochs.update(at + delta for delta in (-1, 0, 1))
                    # Midpoint on either side of both actual wall transitions:
                    # includes each catalog's skipped/repeated local interval.
                    for off in (parsed[0], parsed[1]):
                        wall = EPOCH + timedelta(seconds=at + off + (parsed[1] - parsed[0]) // 2)
                        c = Civil(wall.year, wall.month, wall.day, wall.hour, wall.minute, wall.second, 255)
                        inverse = Inverse()
                        status = lib.portable_timezone_local_to_utc(C.byref(rule), C.byref(c), C.byref(inverse))
                        wall_epoch = int((wall - EPOCH).total_seconds())
                        candidates = sorted({wall_epoch - offset for offset in parsed[:2]
                                             if expected(parsed, wall_epoch - offset)[0] == wall})
                        assert status == {0: 3, 1: 0, 2: 2}[len(candidates)], (zone, wall, status, candidates)
                        assert [inverse.candidate[i].epoch for i in range(inverse.count)] == candidates
                        vectors += 1
            for utc in epochs:
                want, offset, dst = expected(parsed, utc)
                got = Civil()
                detail = Candidate()
                assert lib.portable_timezone_utc_to_local(C.byref(rule), utc, C.byref(got), C.byref(detail)) == 0
                assert (got.year, got.month, got.day, got.hour, got.minute, got.second, got.weekday) == (
                    want.year, want.month, want.day, want.hour, want.minute, want.second, (want.weekday() + 1) % 7), (zone, utc)
                assert detail.offset == offset and detail.daylight == dst
                vectors += 1
    print(f'Independent Python calendar oracle: {vectors} transition/season/fold/gap vectors across all 419 catalog entries passed')
    return vectors


def xtensa(cc, directory):
    version = subprocess.check_output([cc, '--version'], text=True).splitlines()[0]
    assert '8.4.0' in version and '2021r2-patch5' in version, version
    readelf, strip = cc.replace('gcc', 'readelf'), cc.replace('gcc', 'strip')
    flags = [cc, '-std=c11', '-Os', '-fPIC', '-mtext-section-literals', '-mlongcalls',
             '-fvisibility=hidden', '-Wall', '-Wextra', '-Werror', '-I' + str(INCLUDE)]
    for source in SOURCES:
        run([*flags, '-c', source, '-o', directory / (source.stem + '.o')])
    elf = directory / 'portable-timezone.elf'
    run([*flags, '-nostdlib', '-nostartfiles', '-shared', *SOURCES,
         ROOT / 'test/native_apps/fixtures/portable_timezone_elf.c', '-Wl,--hash-style=sysv', '-o', elf])
    run([strip, '--strip-unneeded', elf])
    symbols = subprocess.check_output([readelf, '--dyn-syms', '--wide', str(elf)], text=True)
    imports = validate_imports(symbols, firmware_exports(ROOT))
    assert imports <= {'memcpy', 'memset'}, imports
    assert any('GLOBAL' in line and 'FUNC' in line and line.split()[-1] == 'app_main' for line in symbols.splitlines())
    validator = directory / 'validate'
    run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
         '-I' + str(ROOT / 'test/native_apps/stubs'), '-I' + str(ROOT / 'lib/elf_loader/include'),
         ROOT / 'lib/elf_loader/src/esp_elf_validate.c', ROOT / 'test/native_apps/validate_test.c', '-o', validator])
    run([validator, elf])
    result = {'compiler': version, 'compiler_sha256': hashlib.sha256(Path(cc).read_bytes()).hexdigest(),
              'elf_bytes': elf.stat().st_size, 'elf_sha256': hashlib.sha256(elf.read_bytes()).hexdigest(),
              'imports': sorted(imports), 'loader_validation': 'PASS', 'dynamic_symbols': symbols}
    print('Xtensa pinned object/ELF proof:', version, result['elf_bytes'], 'bytes; imports:', ', '.join(sorted(imports)))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--xtensa-cc', help='Pinned 8.4.0+2021r2-patch5 compiler; no implicit toolchain substitution')
    parser.add_argument('--evidence', type=Path, help='Optional JSON receipt (no product artifact)')
    args = parser.parse_args()
    evidence = {'host_plain': 'PASS', 'host_asan_ubsan': 'PASS'}
    with tempfile.TemporaryDirectory(prefix='portable-timezone-') as temp:
        directory = Path(temp)
        for sanitized in (False, True):
            executable = directory / ('sanitized' if sanitized else 'normal')
            flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie'] if sanitized else []
            run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', *flags,
                 '-I' + str(INCLUDE), *SOURCES, ROOT / 'test/native_apps/portable_timezone_test.c', '-o', executable])
            run([executable], env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0'), timeout=30)
        for denver in (False, True):
            for sanitized in (False, True):
                flags = ['-DPORTABLE_RTC_UTC8_DENVER'] if denver else []
                if sanitized:
                    flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie']
                executable = directory / 'watch-regression'
                run([os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-Wall', '-Wextra', '-Werror', *flags,
                     '-I' + str(INCLUDE), ROOT / 'test/native_apps/portable_time_test.c', '-o', executable])
                run([executable], env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0'), timeout=60)
        evidence['watch_default_and_denver_plain_sanitized'] = 'PASS'
        shared = directory / 'oracle.so'
        run([os.environ.get('CC', 'cc'), '-std=c11', '-O2', '-fPIC', '-shared', '-I' + str(INCLUDE), *SOURCES, '-o', shared])
        evidence['independent_oracle_vectors'] = oracle(shared)
        if args.xtensa_cc:
            evidence['xtensa'] = xtensa(args.xtensa_cc, directory)
        else:
            evidence['xtensa'] = 'NOT RUN (supply --xtensa-cc)'
        # Existing Watch compile-time policy must remain byte-for-byte unchanged.
        provenance = json.loads((ROOT / 'lib/PortableApps/time/TIMEZONE_PROVENANCE.json').read_text())
        for relative, digest in provenance['unchanged_watch_files'].items():
            assert hashlib.sha256((ROOT / relative).read_bytes()).hexdigest() == digest, relative
        evidence['unchanged_watch_files'] = 'PASS'
    if args.evidence:
        args.evidence.parent.mkdir(parents=True, exist_ok=True)
        args.evidence.write_text(json.dumps(evidence, indent=2) + '\n')


if __name__ == '__main__':
    main()

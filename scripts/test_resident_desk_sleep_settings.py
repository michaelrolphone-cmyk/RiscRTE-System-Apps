#!/usr/bin/env python3
"""Execute selected resident Settings definitions, controller and real rasters."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import build_portable_settings as builder

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--normal-only', action='store_true')
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    record = json.loads((args.target / 'settings-build-record.json').read_text())
    assert record['version'] == '1.3.23'
    assert record['desk_clock_default'] == 'Points in Time'
    assert len(record['desk_clock_faces']) == 7
    assert record['time_to_sleep']['owner'] == 'resident host'
    assert record['resident_shell']['role'] == 'foreground'
    assert not record['quick_actions'] and not record['mode_capabilities']['sleep_backend']
    for name, digest in record['source_sha256'].items():
        assert hashlib.sha256((ROOT / name).read_bytes()).hexdigest() == digest, name
    include = args.target.resolve() / 'native-time-sdk/include'
    fixture_defines = set('PORTABLE_SETTINGS_APP PORTABLE_SETTINGS_NATIVE_TIME PORTABLE_SETTINGS_TIME_ZONE '
        'PORTABLE_SETTINGS_X4_DESK_CLOCK PORTABLE_SLEEP_SETTINGS PORTABLE_INPUT_NAVIGATION PORTABLE_ALARM_CLIENT'.split())
    flags = [flag for flag in record['build_defines'] if flag[2:] not in fixture_defines]
    flags.append('-DTEST_NATIVE_SETTINGS_ALARMS')
    sources = [ROOT / 'Apps/settings_native_entry.c', ROOT / 'test/native_apps/resident_desk_sleep_settings_test.c',
               *[ROOT / name for name in builder.NATIVE_TIME_SOURCES]]
    cases = [('defaults', 0), ('root-positions', 0)]
    cases += [(name, value) for value in range(7) for name in ('face-read', 'face-save', 'face-read-save')]
    cases += [(name, value) for value in range(2) for name in ('direction-read', 'direction-save', 'direction-read-save')]
    cases += [(name, 0) for name in ('face-invalid', 'face-unavailable', 'face-write-fail', 'face-verify-fail',
              'direction-invalid', 'direction-unavailable', 'direction-write-fail', 'direction-verify-fail',
              'timer-save', 'timer-bottom-tap', 'timer-invalid', 'timer-unavailable', 'timer-write-fail', 'timer-verify-fail',
              'face-cancel', 'face-home', 'direction-cancel', 'direction-home', 'timer-cancel', 'timer-home')]
    cases += [('timer-read', value) for value in (5, 7, 60, 123, 3600)]
    cases += [(name, value) for value in (5, 7, 3600) for name in ('timer-down', 'timer-up')]
    runs = []
    for short in (False, True):
        for sanitized in ([False] if args.normal_only else [False, True]):
            label = ('short' if short else 'paper') + ('-san' if sanitized else '')
            extra = ['-DTEST_NATIVE_SETTINGS_SHORT'] if short else []
            if sanitized:
                extra += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie']
            binary = out / label
            subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                '-Wno-unused-function', *flags, *extra, '-I' + str(include), '-I' + str(ROOT / 'lib/NativeApps/include'),
                *map(str, sources), '-Wl,--wrap=free', '-o', str(binary)], check=True)
            for name, value in cases:
                frames = out / (label + '-frames') / (name + '-' + str(value))
                frames.mkdir(parents=True, exist_ok=True)
                result = subprocess.run([str(binary), name, str(value)], capture_output=True, text=True, timeout=20,
                    env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0', 'UBSAN_OPTIONS': 'halt_on_error=1:print_stacktrace=1',
                         'NATIVE_SETTINGS_FRAMES': str(frames)})
                (frames / 'run.log').write_text(result.stdout + result.stderr)
                assert result.returncode == 0, (label, name, value, result.stdout, result.stderr)
                runs.append({'profile': label, **json.loads(result.stdout)})
            print(label + ': ' + str(len(cases)) + ' actual Settings cases passed', flush=True)
    if not args.normal_only:
        for label in ('paper', 'short'):
            for path in (out / (label + '-frames')).rglob('*.pbm'):
                peer = out / (label + '-san-frames') / path.relative_to(out / (label + '-frames'))
                assert path.read_bytes() == peer.read_bytes(), path
    receipt = {'target_sha256': record['sha256'], 'runs': runs, 'hardware': 'not run',
        'scope': 'actual selected Settings app/controller/raster; capability providers and resident checkpoint are simulated',
        'source_sha256': {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                          for path in [*sources, Path(__file__)]}}
    (out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(str(len(runs)) + ' selected Settings cases PASS')


if __name__ == '__main__':
    main()

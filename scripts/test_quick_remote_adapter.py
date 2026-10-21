#!/usr/bin/env python3
"""Real resident Quick adapter/session/renderer, canonical SDK, fake radio only."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/quick-remote-adapter')
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--drivers', type=Path, required=True)
    parser.add_argument('--utilities', type=Path, required=True)
    parser.add_argument('--x4', type=Path, required=True)
    parser.add_argument('--normal-only', action='store_true')
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    sdk = out / 'include'
    sdk.mkdir(exist_ok=True)
    for obsolete in sdk.glob("*.h"):
        obsolete.unlink()
    # All included contracts live in one directory, avoiding mixed pragma-once
    # copies when a portable wrapper itself includes a canonical SDK header.
    sources = {}
    roots = [ROOT / 'lib/PortableApps/include',
             args.runtime / 'sdk/app', args.runtime / 'sdk/driver',
             args.runtime / 'sdk/hardware', args.drivers / 'sdk/driver',
             args.utilities / 'lib/Alarm/include']
    for directory in roots:
        for source in directory.glob('*.h'):
            sources[source.name] = source.resolve()
    for name in ('RiscDisplayOutputV1.h', 'RiscDisplayOutputPowerV1.h',
                 'RiscDisplayOutputMetricsV1.h', 'RiscDisplayOutputSnapshotV1.h'):
        sources[name] = (args.x4 / 'minimal/t039/profile/target-x4/sdk/include' / name).resolve()
    for required in ('RiscRuntimeV1.h', 'RiscResidentShellV1.h',
                     'RiscBluetoothSensorsV1.h', 'AlarmServiceV1.h'):
        assert required in sources, required
    for name, source in sources.items():
        shutil.copyfile(source, sdk / name)
    # PortableTime.h has a relative include into the shared timezone directory.
    shutil.copytree(ROOT / 'lib/PortableApps/time', out / 'time', dirs_exist_ok=True)
    defines = ['PORTABLE_QUICK_REMOTE_BATTERY', 'PORTABLE_QUICK_ACTIONS',
               'PORTABLE_QUICK_RADIOS', 'PORTABLE_RESIDENT_SHELL_HOST',
               'PORTABLE_PAPER_TRANSITIONS', 'PORTABLE_NATIVE_CUSTODY_FENCE',
               'PORTABLE_ALARM_CLIENT', 'PORTABLE_ALARM_TERMINAL_RETENTION',
               'ALARM_SERVICE_TAGGED_V2']
    code = [ROOT / 'test/native_apps/quick_remote_adapter_test.c']
    code += [ROOT / 'lib/PortableApps/src' / name for name in (
        'quick_actions.c', 'quick_render.c', 'quick_session.c', 'quick_radios.c', 'remote_battery.c')]
    results = []
    first_frames = None
    dependency_hashes = {}
    configurations = [(atomic, sanitized) for atomic in (False, True)
                      for sanitized in ([False] if args.normal_only else [False, True])]
    for atomic, sanitized in configurations:
        label = ('atomic-' if atomic else '') + ('sanitized' if sanitized else 'normal')
        binary = out / label
        flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                 '-fno-omit-frame-pointer', '-no-pie'] if sanitized else []
        command = [os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g',
                   '-Wall', '-Wextra', '-Werror', '-Wno-unused-function',
                   '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections',
                   *flags, *('-D' + value for value in defines),
                   *(['-DPORTABLE_QUICK_ATOMIC_PREFERENCES'] if atomic else []),
                   '-I' + str(sdk), '-I' + str(ROOT / 'lib/NativeApps/include'),
                   *map(str, code), '-o', str(binary)]
        dependency_result = subprocess.run([command[0], '-MM', *command[1:-2]],
                                           text=True, capture_output=True, check=True, timeout=60)
        (out / (label + '.d')).write_text(dependency_result.stdout)
        dependencies = {Path(token).resolve() for token in shlex.split(
            dependency_result.stdout.replace('\\\n', ' ')) if not token.endswith(':')}
        before = {str(source): hashlib.sha256(source.read_bytes()).hexdigest()
                  for source in dependencies}
        built = subprocess.run(command, text=True, capture_output=True, timeout=120)
        (out / (label + '-build.log')).write_text(built.stdout + built.stderr)
        if built.returncode:
            print(built.stdout + built.stderr, flush=True)
        built.check_returncode()
        frames = out / (label + '-frames')
        frames.mkdir(exist_ok=True)
        result = subprocess.run([str(binary), str(frames)], text=True,
                                capture_output=True, timeout=60,
                                env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0',
                                         UBSAN_OPTIONS='halt_on_error=1'))
        (out / (label + '.log')).write_text(result.stdout + result.stderr)
        print(result.stdout + result.stderr, end='', flush=True)
        result.check_returncode()
        frame_hashes = {path.name: hashlib.sha256(path.read_bytes()).hexdigest()
                        for path in sorted(frames.glob('*.pbm'))}
        assert len(frame_hashes) == 7 and len(set(frame_hashes.values())) == 7
        if first_frames is not None:
            assert first_frames == frame_hashes, 'Normal and sanitizer renderer images differ'
        first_frames = frame_hashes
        try:
            from PIL import Image, ImageDraw
        except ImportError:
            pass  # Binary PBM originals are always emitted; PNG is optional.
        else:
            sheet = Image.new('RGB', (480 * 4, 840 * 2), '#dddddd')
            labels = ImageDraw.Draw(sheet)
            for index, path in enumerate(sorted(frames.glob('*.pbm'))):
                with Image.open(path) as picture:
                    picture.save(path.with_suffix('.png'))
                    sheet.paste(picture, (index % 4 * 480, index // 4 * 840 + 40))
                labels.text((index % 4 * 480 + 10, index // 4 * 840 + 15), path.stem, fill='black')
            sheet.save(frames / 'contact-sheet.png')
        assert all(hashlib.sha256(Path(path).read_bytes()).hexdigest() == digest
                   for path, digest in before.items()), 'Compiled source changed during a test run; rerun'
        dependency_hashes.update(before)
        results.append({'configuration': label, 'atomic_queue': atomic, 'sanitized': sanitized, 'passed': True, 'command': command,
                        'frame_sha256': frame_hashes,
                        'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest()})
    (out / 'evidence.json').write_text(json.dumps({
        'runs': results, 'defines': defines, 'hardware_tested': False,
        'compiler_dependency_sha256': dependency_hashes,
        'boundary': 'Actual adapter.c, Quick controller, session, radio policy and paper renderer; fake Runtime and sensor callbacks. No Runtime loader, physical RF or persistent storage.',
        'sdk_sources': {name: {'source': str(source), 'sha256': hashlib.sha256(source.read_bytes()).hexdigest()}
                        for name, source in sorted(sources.items())},
        'source_sha256': {str(source.relative_to(ROOT)): hashlib.sha256(source.read_bytes()).hexdigest()
                          for source in code + [Path(__file__).resolve()]},
    }, indent=2) + '\n')
    print('Real remote Quick adapter: normal and ASan/UBSan passed' if not args.normal_only
          else 'Real remote Quick adapter: normal passed')


if __name__ == '__main__':
    main()

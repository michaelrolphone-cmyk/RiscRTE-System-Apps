#!/usr/bin/env python3
"""Actual opt-in Settings controllers, pixels and feature-off byte witnesses.

Ordinary builders and product profiles remain unchanged. This isolated runner
can link a development-only Xtensa ELF when the shared pinned compiler is given.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BASE = '3bcc9b3accbd8169f0441a6874114127a69507d0'
TZ = ['PortableTimeZone.c', 'PortableTimeZoneCatalog.c', 'PortableTimeZonePreference.c']
PROFILES = [('paper', []), ('short-paper', ['-DTEST_TIMEZONE_SHORT']),
            ('compact', ['-DTEST_TIMEZONE_COMPACT']),
            ('compact-180', ['-DTEST_TIMEZONE_COMPACT', '-DPORTABLE_TOUCH_ROTATION=180'])]


def run(command, **kwargs):
    return subprocess.run(list(map(str, command)), check=True, **kwargs)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def xtensa(cc, out):
    from native_app_symbols import validate_imports
    version = subprocess.check_output([cc, '--version'], text=True).splitlines()[0]
    assert '8.4.0' in version and '2021r2-patch5' in version, version
    catalog = out / 'catalog.c'
    catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
    mapping = out / 'settings.map'
    mapping.write_text('{ global: app_main; app_module_init; app_module_fini; local: *; };\n')
    flags = [cc, '-std=c11', '-Os', '-fPIC', '-mtext-section-literals', '-mlongcalls',
             '-fvisibility=hidden', '-ffreestanding', '-fno-builtin', '-nostdlib', '-nostartfiles', '-shared',
             '-Wl,--hash-style=sysv', '-Wl,--version-script='+str(mapping), '-Wall', '-Wextra', '-Werror',
             '-DPORTABLE_SETTINGS_APP']

    def build(root, name, definitions, timezone=False):
        elf = out / (name+'.elf')
        version_file = 'lib/PortableApps/profiles/x4-desk-clock-settings.json' if '-DPORTABLE_SETTINGS_X4_DESK_CLOCK' in definitions else 'Apps/settings.json'
        version = json.loads((root/version_file).read_text())['version']
        sources = [root/'Apps/settings.c', root/'lib/PortableApps/src/adapter.c', catalog]
        if timezone:
            sources += [root/'lib/PortableApps/src'/s for s in TZ]
        run([*flags, *definitions, '-DPORTABLE_SETTINGS_VERSION=\"'+version+'\"', '-I'+str(root/'lib/PortableApps/include'),
             '-I'+str(root/'lib/NativeApps/include'), *sources, '-o', elf], timeout=120)
        return elf

    with tempfile.TemporaryDirectory(prefix='timezone-settings-baseline-') as temporary:
        base = Path(temporary)
        archive = subprocess.check_output(['git', 'archive', BASE], cwd=ROOT)
        archive_file = base/'base.tar'
        archive_file.write_bytes(archive)
        with tarfile.open(archive_file) as tar:
            tar.extractall(base, filter='data')
        preserved = {}
        for name, definitions in [('watch', []), ('paper', ['-DPORTABLE_DISPLAY_ROTATION=90', '-DPORTABLE_RTC_WALL_TIME']),
                                  ('desk', ['-DPORTABLE_DISPLAY_ROTATION=90', '-DPORTABLE_RTC_WALL_TIME',
                                            '-DPORTABLE_NOVA_UI', '-DPORTABLE_SLEEP_SETTINGS',
                                            '-DPORTABLE_ALARM_SETTINGS', '-DPORTABLE_SETTINGS_X4_DESK_CLOCK'])]:
            before = build(base, name+'-base', definitions)
            after = build(ROOT, name+'-off', definitions)
            assert before.read_bytes() == after.read_bytes(), name+' feature-off ELF changed'
            preserved[name] = {'sha256': digest(after), 'bytes': after.stat().st_size, 'exact_base_match': True}
        elf = build(ROOT, 'timezone-opt-in', ['-DPORTABLE_SETTINGS_TIME_ZONE', '-DPORTABLE_DISPLAY_ROTATION=90',
                                            '-DPORTABLE_RTC_WALL_TIME', '-DPORTABLE_INPUT_NAVIGATION'], True)
        readelf = cc.removesuffix('gcc')+'readelf'
        symbols = subprocess.check_output([readelf, '--dyn-syms', '--wide', elf], text=True)
        # This portable app uses the same import contract as build_portable_settings.py.
        # The historical Reader SDK list predates risc_runtime_get_api.
        allowed = {'risc_runtime_get_api', 'memcpy', 'memset', 'memcmp', 'strcmp', 'strlen', 'snprintf', 'strcpy', 'malloc', 'free'}
        imports = validate_imports(symbols, allowed)
        validator = out/'validate'
        run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
             '-I'+str(ROOT/'test/native_apps/stubs'), '-I'+str(ROOT/'lib/elf_loader/include'),
             ROOT/'lib/elf_loader/src/esp_elf_validate.c', ROOT/'test/native_apps/validate_test.c', '-o', validator])
        run([validator, elf])
        return {'compiler': version, 'compiler_sha256': digest(Path(cc)), 'feature_off': preserved,
                'opt_in': {'sha256': digest(elf), 'bytes': elf.stat().st_size, 'imports': sorted(imports), 'loader_validation': 'PASS'}}


def rasterize(out):
    from PIL import Image, ImageDraw
    result = {}
    for profile, _ in PROFILES:
        if profile == 'compact-180':
            continue
        destination = out/'pixels'/profile
        destination.mkdir(parents=True, exist_ok=True)
        frames = []
        for source in sorted((out/'frames'/profile).glob('*')):
            frame = Image.open(source)
            frame.load()
            if source.suffix == '.pbm':
                frame = frame.transpose(Image.Transpose.ROTATE_270)
            path = destination/(source.stem+'.png')
            frame.save(path)
            assert Image.open(path).tobytes() == frame.tobytes()
            result[str(path.relative_to(out))] = {'width': frame.width, 'height': frame.height,
                                                 'sha256': digest(path), 'pixel_sha256': hashlib.sha256(frame.tobytes()).hexdigest()}
            # Last rendered frame per scenario is a useful bounded contact sheet.
            if not frames or source.name.split('-')[0] != frames[-1][0].split('-')[0]:
                frames.append((source.stem, frame))
            elif int(source.stem.rsplit('-', 1)[-1]) > int(frames[-1][0].rsplit('-', 1)[-1]):
                frames[-1] = (source.stem, frame)
        width, height = frames[0][1].size
        sheet = Image.new('RGB', (width*4, (height+28)*((len(frames)+3)//4)), '#bbbbbb')
        draw = ImageDraw.Draw(sheet)
        for i, (name, frame) in enumerate(frames):
            x, y = i%4*width, i//4*(height+28)
            draw.text((x+8, y+6), name, fill='black')
            sheet.paste(frame, (x, y+28))
        sheet.save(out/(profile+'-contact-sheet.png'))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--xtensa-cc')
    parser.add_argument('--evidence', type=Path)
    parser.add_argument('--pixels', action='store_true', help='Capture and verify real production framebuffer PNGs (requires Pillow)')
    args = parser.parse_args()
    out = ROOT/'build/timezone-settings'
    out.mkdir(parents=True, exist_ok=True)
    environment = dict(os.environ, ASAN_OPTIONS='detect_leaks=0')
    evidence = {'base_commit': BASE, 'catalog_entries': 419, 'profiles': {}, 'host_compiler': subprocess.check_output([os.environ.get('CC', 'cc'), '--version'], text=True).splitlines()[0]}
    for name, profile in PROFILES:
        for sanitized in (False, True):
            executable = out/(name+('-san' if sanitized else ''))
            flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie'] if sanitized else []
            run([os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-Wall', '-Wextra', '-Werror', *flags, *profile,
                 '-I'+str(ROOT/'lib/PortableApps/include'), '-I'+str(ROOT/'lib/NativeApps/include'),
                 ROOT/'Apps/settings.c', *[ROOT/'lib/PortableApps/src'/s for s in TZ],
                 ROOT/'test/native_apps/portable_timezone_settings_test.c', '-o', executable], timeout=120)
            with (out/(executable.name+'.log')).open('w') as log:
                for mode in (0, 1):
                    for index in range(419):
                        run([executable, mode, index], stdout=log, env=environment, timeout=15)
                for mode in range(2, 30):
                    run([executable, mode], stdout=log, env=environment, timeout=15)
                if args.pixels and not sanitized and name != 'compact-180':
                    frames = out/'frames'/name
                    frames.mkdir(parents=True, exist_ok=True)
                    for mode in range(30, 37):
                        run([executable, mode], stdout=log, env=dict(environment, TIMEZONE_SETTINGS_FRAMES=str(frames)), timeout=15)
            evidence['profiles'][executable.name] = {'all_choices_navigation': 419, 'all_choices_touch': 419,
                                                     'boundary_and_fault_scenarios': 28, 'result': 'PASS'}
            print(executable.name+': all 419 zones by navigation and touch, plus 28 boundary/fault scenarios passed', flush=True)
    if args.xtensa_cc:
        evidence['xtensa'] = xtensa(args.xtensa_cc, out)
    if args.pixels:
        evidence['rendered_pixels'] = rasterize(out)
    evidence['source_sha256'] = {str(path.relative_to(ROOT)): digest(path) for path in [
        ROOT/'Apps/settings.c', ROOT/'lib/PortableApps/src/adapter.c', ROOT/'lib/PortableApps/src/settings.inc',
        ROOT/'lib/PortableApps/src/settings_view.inc', ROOT/'lib/PortableApps/src/settings_timezone.inc',
        ROOT/'lib/PortableApps/src/settings_timezone_view.inc', ROOT/'test/native_apps/portable_timezone_settings_test.c',
        ROOT/'scripts/test_timezone_settings.py', *[ROOT/'lib/PortableApps/src'/s for s in TZ]]}
    if args.evidence:
        args.evidence.parent.mkdir(parents=True, exist_ok=True)
        args.evidence.write_text(json.dumps(evidence, indent=2)+'\n')
    print('Timezone Settings: 6,928 controller executions passed; no product/BIN changes')


if __name__ == '__main__':
    main()

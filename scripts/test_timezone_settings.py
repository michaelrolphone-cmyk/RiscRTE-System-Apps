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
import shlex
import shutil
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
    from timezone_settings_witness import (POISON, assert_identical,
                                           assert_timezone_absent, prepare_projection)
    version = subprocess.check_output([cc, '--version'], text=True).splitlines()[0]
    assert '8.4.0' in version and '2021r2-patch5' in version, version
    compiler_hash = digest(Path(cc))
    catalog = out / 'catalog.c'
    catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
    mapping = out / 'settings.map'
    mapping.write_text('{ global: app_main; app_module_init; app_module_fini; local: *; };\n')
    flags = [cc, '-std=c11', '-Os', '-fPIC', '-mtext-section-literals', '-mlongcalls',
             '-fvisibility=hidden', '-ffreestanding', '-fno-builtin', '-Wall', '-Wextra', '-Werror',
             '-DPORTABLE_SETTINGS_APP']
    link = ['-nostdlib', '-nostartfiles', '-shared', '-Wl,--hash-style=sysv',
            '-Wl,--version-script='+str(mapping)]
    profiles = [('watch', []), ('paper', ['-DPORTABLE_DISPLAY_ROTATION=90', '-DPORTABLE_RTC_WALL_TIME']),
                ('desk', ['-DPORTABLE_DISPLAY_ROTATION=90', '-DPORTABLE_RTC_WALL_TIME',
                          '-DPORTABLE_NOVA_UI', '-DPORTABLE_SLEEP_SETTINGS',
                          '-DPORTABLE_ALARM_SETTINGS', '-DPORTABLE_SETTINGS_X4_DESK_CLOCK'])]
    opt_in_definitions = ['-DPORTABLE_SETTINGS_TIME_ZONE', '-DPORTABLE_DISPLAY_ROTATION=90',
                          '-DPORTABLE_RTC_WALL_TIME', '-DPORTABLE_INPUT_NAVIGATION']
    readelf, nm = cc.removesuffix('gcc')+'readelf', cc.removesuffix('gcc')+'nm'
    validator = out/'validate'
    run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
         '-I'+str(ROOT/'test/native_apps/stubs'), '-I'+str(ROOT/'lib/elf_loader/include'),
         ROOT/'lib/elf_loader/src/esp_elf_validate.c', ROOT/'test/native_apps/validate_test.c', '-o', validator])
    # Same portable Settings import contract as build_portable_settings.py.
    allowed = {'risc_runtime_get_api', 'memcpy', 'memset', 'memcmp', 'strcmp', 'strlen', 'snprintf', 'strcpy', 'malloc', 'free'}
    source_inputs, manifest_versions = {}, {}

    def arguments(root, definitions):
        version_file = 'lib/PortableApps/profiles/x4-desk-clock-settings.json' if '-DPORTABLE_SETTINGS_X4_DESK_CLOCK' in definitions else 'Apps/settings.json'
        app_version = json.loads((root/version_file).read_text())['version']
        if root == ROOT:
            manifest_versions[version_file] = app_version
        return [*flags, *definitions, '-DPORTABLE_SETTINGS_VERSION="'+app_version+'"',
                '-I'+str(root/'lib/PortableApps/include'), '-I'+str(root/'lib/NativeApps/include')]

    def preprocess(root, definitions, source):
        return subprocess.check_output([*arguments(root, definitions), '-E', '-P', root/source])

    def build(root, name, definitions, timezone=False):
        elf = out / (name+'.elf')
        sources = [root/'Apps/settings.c', root/'lib/PortableApps/src/adapter.c', catalog]
        if timezone:
            sources += [root/'lib/PortableApps/src'/s for s in TZ]
        run([*arguments(root, definitions), *link, *sources, '-o', elf], timeout=120)
        if root == ROOT:
            # Record the complete repository-local compiler dependency closure,
            # including adapter .inc files and headers, not just top-level C.
            for source in sources:
                dependencies = subprocess.check_output(
                    [*arguments(root, definitions), '-MM', '-MT', 'witness', source], text=True)
                for dependency in shlex.split(dependencies.replace('\\\n', '').split(':', 1)[1]):
                    path = Path(dependency).resolve()
                    if path.is_relative_to(ROOT) and not path.is_relative_to(out):
                        source_inputs[str(path.relative_to(ROOT))] = digest(path)
            for path in (ROOT/'Apps/settings.json', ROOT/'lib/PortableApps/profiles/x4-desk-clock-settings.json'):
                source_inputs[str(path.relative_to(ROOT))] = digest(path)
        return elf

    def validate(elf, feature_off):
        symbols = subprocess.check_output([readelf, '--dyn-syms', '--wide', elf], text=True)
        imports = validate_imports(symbols, allowed)
        exports = {fields[7] for line in symbols.splitlines()
                   if len(fields := line.split()) >= 8 and fields[4] in {'GLOBAL', 'WEAK'}
                   and fields[6] != 'UND'}
        assert exports == {'app_main', 'app_module_init', 'app_module_fini'}, exports
        names = subprocess.check_output([nm, '--defined-only', elf], text=True)
        if feature_off:
            assert_timezone_absent(names, elf.name+' symbols')
        else:
            assert any(line.split()[-1].startswith('stz_') for line in names.splitlines())
            assert 'portable_timezone_preference_save' in names, 'opt-in timezone persistence missing'
        run([validator, elf])
        return {'sha256': digest(elf), 'bytes': elf.stat().st_size,
                'imports': sorted(imports), 'exports': sorted(exports), 'loader_validation': 'PASS'}

    receipt_path = ROOT/'docs/desk-clock/timezone-settings-evidence/evidence.json'
    historical = json.loads(receipt_path.read_text())
    assert historical['base_commit'] == BASE
    historical_sources = {
        path: {'recorded_sha256': old, 'current_sha256': digest(ROOT/path),
               'matches_recorded_source': digest(ROOT/path) == old}
        for path, old in historical['source_sha256'].items()}
    preserved = {}
    with tempfile.TemporaryDirectory(prefix='timezone-settings-projection-') as temporary:
        projection = Path(temporary)
        for directory in ('Apps', 'lib'):
            shutil.copytree(ROOT/directory, projection/directory)
        # First poison only: enabled preprocessing must genuinely reach the
        # unavailable feature files. A typo or a disconnected gate cannot pass.
        prepare_projection(projection, poison_only=True)
        for name, definitions in profiles:
            control = subprocess.run([*arguments(projection, [*definitions, '-DPORTABLE_SETTINGS_TIME_ZONE']),
                                      '-E', '-P', projection/'lib/PortableApps/src/adapter.c'],
                                     capture_output=True, text=True, timeout=120)
            assert control.returncode != 0 and POISON in control.stderr, name+' poison control did not fail'
        gates = prepare_projection(projection)
        for name, definitions in profiles:
            # Force the flag ON in the erased copy: equality must come from
            # physical feature removal, not the same disabled #ifdef twice.
            erased_definitions = [*definitions, '-DPORTABLE_SETTINGS_TIME_ZONE']
            preprocessed = {}
            for source in ('Apps/settings.c', 'lib/PortableApps/src/adapter.c'):
                before = preprocess(ROOT, definitions, source)
                after = preprocess(projection, erased_definitions, source)
                assert_timezone_absent(before.decode(), name+' '+source)
                assert_identical(before, after, name+' '+source+' preprocessed source')
                preprocessed[source] = hashlib.sha256(before).hexdigest()
            current = build(ROOT, name+'-off', definitions)
            erased = build(projection, name+'-erased', erased_definitions)
            assert_identical(current.read_bytes(), erased.read_bytes(), name+' ELF')
            result = validate(current, True)
            validate(erased, True)
            recorded = historical['xtensa']['feature_off'][name]
            result.update({'exact_current_projection_match': True,
                           'erased_projection_feature_flag': 'ENABLED',
                           'preprocessed_sha256': preprocessed,
                           'timezone_identifiers_absent': True,
                           'enabled_poison_control': 'EXPECTED_FAILURE',
                           'recorded_historical_sha256': recorded['sha256'],
                           'matches_recorded_historical_elf': result['sha256'] == recorded['sha256']})
            preserved[name] = result
            print(name+': current feature-off tokens and ELF match erased projection; imports/exports/loader PASS', flush=True)
        elf = build(ROOT, 'timezone-opt-in', opt_in_definitions, True)
        opt_in = validate(elf, False)
    return {'compiler': version, 'compiler_sha256': compiler_hash,
            'historical_provenance': {'base_commit': BASE, 'receipt': str(receipt_path.relative_to(ROOT)),
                'receipt_sha256': digest(receipt_path), 'replayed_this_run': False,
                'recorded_compiler_sha256': historical['xtensa']['compiler_sha256'],
                'matches_recorded_compiler': compiler_hash == historical['xtensa']['compiler_sha256'],
                'recorded_manifest_versions': historical['xtensa']['manifest_versions'],
                'source_comparison': historical_sources},
            'erased_timezone_gates': gates, 'feature_off': preserved, 'opt_in': opt_in,
            'manifest_versions': manifest_versions, 'opt_in_definitions': opt_in_definitions,
            'source_inputs_sha256': dict(sorted(source_inputs.items())),
            'compile_flags': flags[1:], 'link_flags': [item.replace(str(out), '<output>') for item in link],
            'feature_off_definitions': dict(profiles),
            'generated_inputs_sha256': {path.name: digest(path) for path in (catalog, mapping)}}


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
        ROOT/'scripts/test_timezone_settings.py', ROOT/'scripts/timezone_settings_witness.py', *[ROOT/'lib/PortableApps/src'/s for s in TZ]]}
    if args.evidence:
        args.evidence.parent.mkdir(parents=True, exist_ok=True)
        args.evidence.write_text(json.dumps(evidence, indent=2)+'\n')
    print('Timezone Settings: 6,928 controller executions passed; no product/BIN changes')


if __name__ == '__main__':
    main()

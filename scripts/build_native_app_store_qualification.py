#!/usr/bin/env python3
"""Build the explicitly selected native App Store software-qualification app.

This composes current System-Apps with immutable generic capability bridges.
It does not reconstruct a historical product ELF or assemble/install firmware.
The ordinary portable App Store builder and its defaults remain independent.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

from native_toolchain import compiler_path
from report_native_settings_memory import inspect as inspect_elf

ROOT = Path(__file__).resolve().parents[1]
PINS = {
    'bridges': '6e82fdcaec0b73ed700d2104cfb4d94f011c19d4',
    'runtime': '48229bd1593a2e24e95bf4537b495ee615990fd3',
    'profile_source': 'b9862e1379813a0914d4926ef59a3feb6fcf656a',
    'utilities': 'e80172353fcb9e6519ca9d9b2a4d43c69c1bc91d',
}
REPOSITORIES = {
    'bridges': 'michaelrolphone-cmyk/RiscRTE-Watch',
    'runtime': 'michaelrolphone-cmyk/RiscRTE',
    'profile_source': 'michaelrolphone-cmyk/RiscRTE-Wavshare-S3-Touch-AMOLED-2.06',
    'utilities': 'michaelrolphone-cmyk/RiscRTE-Utilities',
}
RECIPE = 'docs/shared-app-crashes/replay/wave.json'
# Immutable retained full-feature recipe, also used by Wave's .55 footer fix.
RECIPE_COMMIT = '297c38619a867f6200cc2f605b6193e4394bdfa2'
RECIPE_SHA256 = '5dd7876cd3a869e0326979b599bf9d2c1201b656606028b4a83a31992553f715'
APP_MANIFEST = 'lib/PortableApps/profiles/native-app-store-manifest.json'
VERSION_PROFILE = 'lib/PortableApps/profiles/native-app-store.json'
DEFINES = (
    'PORTABLE_QUICK_NATIVE', 'PORTABLE_ALARM_NATIVE',
    'PORTABLE_UPDATE_NATIVE_UI', 'PORTABLE_NATIVE_CANVAS',
           'PORTABLE_NATIVE_CUSTODY_FENCE')
# This declaration covers the complete delivered feature recipe. Namespace
# bindings remain deployment configuration, with required IDs recorded below.
CAPABILITIES = (
    ('display.output', 1), ('input.touch.raw', 1), ('rtc.clock', 2),
    ('alarm.service', 1), ('telemetry.broadcast', 1), ('contexts.service', 1),
    ('input.power-key', 1), ('power.sleep', 1), ('display.power', 1),
    ('board.battery', 1), ('storage.key-value', 1), ('net.wifi', 1),
    ('bluetooth.hci', 1), ('motion.accel', 1), ('software.update.apps', 1),
    ('storage.key-value.atomic', 1),
)
CAPABILITY_SELECTORS = dict(zip((cap for cap, _ in CAPABILITIES), (
    'PORTABLE_UPDATE_APP', 'PORTABLE_UPDATE_APP', 'PORTABLE_RTC_UTC8_DENVER',
    'PORTABLE_ALARM_CLIENT', 'PORTABLE_BLE_BROADCAST', 'PORTABLE_CONTEXTS_CLIENT',
    'PORTABLE_INPUT_NAVIGATION_LOCAL', 'PORTABLE_APP_SLEEP_LOCAL', 'PORTABLE_APP_SLEEP_LOCAL',
    'PORTABLE_LOW_BATTERY', 'PORTABLE_CHECKED_STORAGE_RUNTIME', 'PORTABLE_UPDATE_APP',
    'PORTABLE_QUICK_RADIOS', 'PORTABLE_MOTION_WAKE', 'PORTABLE_UPDATE_APP',
    'PORTABLE_QUICK_ATOMIC_PREFERENCES')))
REQUIRED_FEATURES = set(CAPABILITY_SELECTORS.values()) | set(DEFINES) | {
    'PORTABLE_NOVA_UI', 'PORTABLE_INPUT_NAVIGATION', 'PORTABLE_NAVIGATION_RETENTION',
    'PORTABLE_QUICK_ACTIONS', 'PORTABLE_QUICK_RADIOS', 'PORTABLE_ASYNC_RGB565',
    'PORTABLE_FORCE_FULL_FRAMES', 'WATCH_ALARM_SLEEP_RESUME',
    'PORTABLE_HOME_AUTOMATIC_PREFERENCES', 'PORTABLE_ALARM_TERMINAL_RETENTION',
    'PORTABLE_WATCH_COLOR_ONLY', 'PORTABLE_WATCH_NATIVE_HEIGHT',
}
REQUIRED_VALUES = {
    'PORTABLE_UPDATE_FIRMWARE': '0', 'PORTABLE_WIFI_INSTANCE': '15',
    'PORTABLE_UPDATE_RTC_UTC_OFFSET_SECONDS': '28800',
    'UPDATE_RETURN_APP': '"springboard.elf"', 'PORTABLE_TOUCH_ROTATION': '0',
    'PORTABLE_DISPLAY_ROTATION': '0',
}
REQUIRED_SOURCES = {'app_store.c', 'adapter.c', 'portable_navigation.c',
                    'portable_sleep.c', 'quick_actions.c', 'quick_render.c',
                    'quick_session.c', 'quick_radios.c', 'empty_catalog.c'}
CATALOG_PROFILE = 'profiles/native-app-store/empty_catalog.c'
EXPORTS = {'app_main', 'app_module_init', 'app_module_fini'}
ALLOWED_IMPORTS = {'risc_runtime_get_api', 'memcpy', 'memset', 'memcmp',
    'strcmp', 'strlen', 'snprintf', 'malloc', 'calloc', 'free', 'strcpy',
    'memchr', 'strncmp'}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args])


def pinned(repos, owner, path):
    return git(repos[owner], 'show', PINS[owner]+':'+path)


def manifest(version):
    return {'type': 'application', 'id': 'app_store', 'version': version,
            'architecture': 'xtensa-esp32s3', 'file_name': 'app_store.elf',
            'entry': 'app_main', 'requires': [
                {'capability': cap, 'api': api} for cap, api in CAPABILITIES]}


def canonical_arguments(arguments):
    """Normalize only materialized paths; preserve every option and its order."""
    result = []
    args = list(arguments)
    if args and not args[0].startswith('-'):
        args = args[1:]
    if '-o' in args:
        pos = args.index('-o')
        del args[pos:pos+2]
    for item in args:
        if item.startswith('-I'):
            result.append('-I')
        elif item.startswith('-Wl,--version-script='):
            result.append('-Wl,--version-script=app_store.map')
        elif item.endswith(('.c', '.h')):
            result.append(Path(item).name)
        else:
            result.append(item)
    return result


def validate_recipe(arguments, version):
    original = json.loads((ROOT/RECIPE).read_bytes())['app_store']['command'][1:]
    pos = original.index('-o')
    del original[pos:pos+2]
    expected = original + ['-D'+name for name in DEFINES if '-D'+name not in original]
    expected += ['-DPORTABLE_UPDATE_VERSION="'+version+'"', '-Iprofile', '-Iruntime']
    if canonical_arguments(arguments) != canonical_arguments(expected):
        raise ValueError('Complete retained App Store recipe or selector order changed')
    selected = {item[2:].split('=', 1)[0] for item in arguments if item.startswith('-D')}
    if not REQUIRED_FEATURES <= selected:
        raise ValueError('Complete native App Store feature closure is missing')
    for name, value in {**REQUIRED_VALUES, 'PORTABLE_UPDATE_VERSION': '"'+version+'"'}.items():
        if [item for item in arguments if item.startswith('-D'+name+'=')] != ['-D'+name+'='+value]:
            raise ValueError('Complete native App Store recipe value differs: '+name)
    if {Path(item).name for item in arguments if item.endswith('.c')} != REQUIRED_SOURCES:
        raise ValueError('Complete native App Store source selection changed')
    # Every materialized include/source path must belong to the same recorded
    # System root, immutable bridge checkout or qualification staging directory.
    clean = list(arguments)
    if clean and not clean[0].startswith('-'): clean = clean[1:]
    if '-o' in clean:
        index = clean.index('-o'); del clean[index:index+2]
    roots = [Path(a).parents[1] for a in clean if a.endswith('/Apps/app_store.c')]
    maps = [Path(a.split('=',1)[1]).parent for a in clean if a.startswith('-Wl,--version-script=')]
    bridges = [Path(a).parents[2] for a in clean if a.endswith('/apps/clock/portable_navigation.c')]
    if len(roots) != 1 or len(maps) != 1 or len(bridges) != 1:
        raise ValueError('Ambiguous materialized recipe roots')
    source, out, bridge = roots[0], maps[0], bridges[0]
    replacements = {
        '@r0@/lib/PortableApps/include': str(out/'include'), '@r0@': str(source),
        '@r6@/sdk/app': str(out/'runtime-sdk'), '@r2@/Apps': str(out/'profile'),
        '@r2@/lib/Alarm/include': str(out/'profile'), '@r2@/lib/Contexts/include': str(out/'runtime-sdk'),
        '@r3@': str(bridge), '@work@/target-updaters/input-fixtures/empty_catalog.c': str(source/CATALOG_PROFILE),
        '@work@/target-updaters/input-fixtures': str(out/'profile'),
        '@work@/target-updaters/fixed/app_store.map': str(out/'app_store.map'),
    }
    materialized = []
    for arg in original:
        for old, new in replacements.items(): arg = arg.replace(old,new)
        materialized.append(arg)
    materialized += ['-D'+name for name in DEFINES if '-D'+name not in original]
    materialized += ['-DPORTABLE_UPDATE_VERSION="'+version+'"', '-I'+str(out/'profile'), '-I'+str(out/'runtime-sdk')]
    if clean != materialized:
        raise ValueError('Materialized App Store recipe paths or inputs changed')


def verify_compiler(cc, out):
    probe = out/'target-probe.o'
    command = [cc, '-x', 'c', '-c', '-', '-o', str(probe)]
    subprocess.run(command, input=b'int target_probe;\n', check=True)
    data = probe.read_bytes()
    if data[:7] != b'\x7fELF\x01\x01\x01' or data[16:20] != b'\x01\x00\x5e\x00':
        raise ValueError('Selected compiler does not emit ESP32-S3 little-endian Xtensa objects')
    probe.unlink()
    implementation = {}
    for label, flag in (('cc1', '-print-prog-name=cc1'), ('libgcc', '-print-libgcc-file-name')):
        path = Path(subprocess.check_output([cc, flag], text=True).strip())
        if not path.is_file():
            raise ValueError('Cannot identify compiler implementation: '+label)
        implementation[label] = {'path': str(path), 'sha256': digest(path.read_bytes())}
    return {'compiler_path': cc, 'compiler_target_verified': True,
            'compiler_dumpmachine': subprocess.check_output([cc, '-dumpmachine'], text=True).strip(),
            'compiler_implementation': implementation}


def compose_arguments(recipe_bytes, repos, out, include, profile, runtime_include, version):
    if digest(recipe_bytes) != RECIPE_SHA256:
        raise ValueError('Retained full-feature App Store recipe digest changed')
    original = json.loads(recipe_bytes)['app_store']['command']
    if original[0] != '@cc@' or original.count('-o') != 1:
        raise ValueError('Retained App Store command shape changed')
    output_index = original.index('-o')
    replacements = {
        '@r0@/lib/PortableApps/include': str(include),
        '@r0@': str(ROOT), '@r6@/sdk/app': str(runtime_include),
        '@r2@/Apps': str(profile), '@r2@/lib/Alarm/include': str(profile),
        '@r2@/lib/Contexts/include': str(runtime_include), '@r3@': str(repos['bridges']),
        '@work@/target-updaters/input-fixtures/empty_catalog.c': str(ROOT/CATALOG_PROFILE),
        '@work@/target-updaters/input-fixtures': str(profile),
        '@work@/target-updaters/fixed/app_store.map': str(out/'app_store.map'),
    }
    arguments = []
    for index, item in enumerate(original[1:], 1):
        if index in (output_index, output_index+1):
            continue
        for name, value in replacements.items():
            item = item.replace(name, value)
        if '@' in item:
            raise ValueError('Unresolved retained App Store recipe input: '+item)
        if item.startswith('-DPORTABLE_UPDATE_VERSION='):
            item = '-DPORTABLE_UPDATE_VERSION="'+version+'"'
        arguments.append(item)
    arguments += ['-D'+name for name in DEFINES if '-D'+name not in arguments]
    arguments += ['-DPORTABLE_UPDATE_VERSION=\"'+version+'\"', '-I'+str(profile), '-I'+str(runtime_include)]
    validate_recipe(arguments, version)
    return arguments


def build(args):
    out = args.output_dir.resolve()
    if out.exists() and any(out.iterdir()):
        raise ValueError('Use an empty output directory; stale qualification files are refused')
    repos = {name: getattr(args, name).resolve() for name in PINS}
    # Bridge sources compile in place. Every actual external dependency is
    # compared with its immutable Git object before and after compilation.
    if git(repos['bridges'], 'rev-parse', 'HEAD').decode().strip() != PINS['bridges']:
        raise ValueError('Generic bridge checkout must select '+PINS['bridges'])
    for owner, pin in PINS.items():
        if git(repos[owner], 'cat-file', '-t', pin).strip() != b'commit':
            raise ValueError('Required immutable input is not a commit: '+owner)
    out.mkdir(parents=True, exist_ok=True)
    include = out/'include'
    shutil.copytree(ROOT/'lib/PortableApps/include', include)
    shutil.copytree(ROOT/'lib/PortableApps/time', out/'time')
    staged = {str(p.resolve()): ('system', 'lib/PortableApps/'+str(p.relative_to(out)))
              for base in (include, out/'time') for p in base.rglob('*') if p.is_file()}
    # Quoted System headers see the current Runtime prefix. Other canonical
    # Runtime suffixes are supplied by their separate staged include directory.
    runtime_include = out/'runtime-sdk'
    runtime_include.mkdir()
    for path in git(repos['runtime'], 'ls-tree', '-r', '--name-only',
                    PINS['runtime'], 'sdk/app').decode().splitlines():
        if not path.endswith('.h'): continue
        destination = runtime_include/Path(path).name
        destination.write_bytes(pinned(repos, 'runtime', path))
        staged[str(destination.resolve())] = ('runtime', path)
    for name in ('RiscRuntimeV1.h', 'RiscKeyValueAtomicV1.h', 'RiscKeyValueAvailabilityV1.h'):
        (include/name).write_bytes((runtime_include/name).read_bytes())
        staged[str((include/name).resolve())] = ('runtime', 'sdk/app/'+name)
    profile = out/'profile'
    profile.mkdir()
    profile_inputs = [('profile_source', 'profiles/shared-editor-inputs/'+name)
                      for name in ('watch-alarm-profile.h', 'AlarmServicePinnedV1.h')]
    profile_inputs += [('utilities', 'Apps/alarm_writer.h'),
                       ('utilities', 'lib/Alarm/include/AlarmRecords.h')]
    for owner, path in profile_inputs:
        destination = profile/Path(path).name
        destination.write_bytes(pinned(repos, owner, path))
        staged[str(destination.resolve())] = (owner, path)
    version = json.loads((ROOT/VERSION_PROFILE).read_text())['version']
    recipe_bytes = (ROOT/RECIPE).read_bytes()
    declared_manifest = json.loads((ROOT/APP_MANIFEST).read_text())
    if declared_manifest != manifest(version):
        raise ValueError('Native App Store manifest differs from the complete versioned recipe')
    arguments = compose_arguments(recipe_bytes, repos, out, include, profile, runtime_include, version)
    sources = [Path(item) for item in arguments if item.endswith('.c')]
    preprocessing = []
    for index, item in enumerate(arguments):
        if item.startswith(('-D', '-I')) or item == '-include' or (index and arguments[index-1] == '-include'):
            preprocessing.append(item)
    cc = compiler_path()
    compiler_record = verify_compiler(cc, out)
    dependencies = set()
    for source in sources:
        raw = subprocess.check_output([cc, '-std=c11', '-Os', '-fPIC',
            '-ffreestanding', '-fno-builtin', '-MM', *preprocessing, str(source)], text=True)
        dependencies.update(Path(item).resolve() for item in
                            shlex.split(raw.replace('\\\n', ' ').split(':', 1)[1]))
    source_inputs = {}
    identity_paths = {}
    for path in sorted(dependencies):
        if {'test', 'tests', 'fixtures'} & set(path.parts):
            raise ValueError('Host fixture leaked into target inputs: '+str(path))
        if str(path) in staged:
            owner, relative = staged[str(path)]
        elif path.is_relative_to(ROOT):
            owner, relative = 'system', str(path.relative_to(ROOT))
        else:
            matches = [(key, str(path.relative_to(base))) for key, base in repos.items()
                       if path.is_relative_to(base)]
            if len(matches) != 1:
                raise ValueError('Unowned target input: '+str(path))
            owner, relative = matches[0]
        original = (ROOT/relative).read_bytes() if owner == 'system' else pinned(repos, owner, relative)
        if path.read_bytes() != original:
            raise ValueError('Compiled input differs from selected source: '+str(path))
        identity = owner+':'+relative
        source_inputs[identity] = digest(original)
        identity_paths[path] = (identity, owner, relative)
    required_inputs = {
        'system:lib/PortableApps/src/quick_native_render.inc',
        'system:lib/PortableApps/include/PortableQuickLayout.h',
        'system:lib/PortableApps/include/PortableAlarmLayout.h',
        'system:Apps/update_native.inc', 'system:Apps/update_portable.inc',
        'system:lib/PortableApps/src/wifi_native_view.inc', 'system:'+CATALOG_PROFILE,
        'system:lib/PortableApps/src/native_custody_adapter.inc',
        'system:lib/PortableApps/src/quick_atomic.inc',
        'bridges:apps/clock/portable_navigation.c', 'bridges:apps/clock/portable_sleep.c',
    }
    if not required_inputs <= source_inputs.keys():
        raise ValueError('Complete native App Store source closure is absent: '+repr(required_inputs-source_inputs.keys()))
    metadata_inputs = (VERSION_PROFILE, RECIPE, APP_MANIFEST, CATALOG_PROFILE,
                       'scripts/build_native_app_store_qualification.py', 'scripts/native_toolchain.py',
                       'scripts/report_native_settings_memory.py')
    for relative in metadata_inputs:
        source_inputs['system:'+relative] = digest((ROOT/relative).read_bytes())
    source_commit = git(ROOT, 'rev-parse', 'HEAD').decode().strip()
    (out/'app_store.map').write_text('{ global: '+ '; '.join(sorted(EXPORTS))+'; local: *; };\n')
    elf = out/'app_store.elf'
    command = [cc, *arguments, '-o', str(elf)]
    (out/'command.json').write_text(json.dumps(command, indent=2)+'\n')
    subprocess.run(command, check=True, timeout=120)
    symbols = subprocess.check_output([cc.removesuffix('gcc')+'nm', '-D', str(elf)], text=True)
    imports = {line.split()[-1] for line in symbols.splitlines() if ' U ' in ' '+line}
    exports = {line.split()[-1] for line in symbols.splitlines()
               if len(line.split()) >= 3 and line.split()[-2] in ('T', 'D', 'B', 'R')}
    if not imports <= ALLOWED_IMPORTS or exports != EXPORTS:
        raise ValueError('Unexpected target imports/exports: '+repr((imports, exports)))
    data = elf.read_bytes()
    if data[:7] != b'\x7fELF\x01\x01\x01' or data[16:20] != b'\x03\x00\x5e\x00':
        raise ValueError('Expected ELF32 little-endian Xtensa ET_DYN')
    if (version+'\0').encode() not in data:
        raise ValueError('Selected App Store version is absent from target ELF')
    validator = out/'validate-elf'
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
        '-I'+str(ROOT/'test/native_apps/stubs'), '-I'+str(ROOT/'lib/elf_loader/include'),
        str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'),
        str(ROOT/'test/native_apps/validate_test.c'), '-o', str(validator)], check=True)
    subprocess.run([str(validator), str(elf)], check=True)
    for path, (identity, owner, relative) in identity_paths.items():
        original = (ROOT/relative).read_bytes() if owner == 'system' else pinned(repos, owner, relative)
        if digest(path.read_bytes()) != source_inputs[identity] or digest(original) != source_inputs[identity]:
            raise ValueError('Source changed during target qualification: '+identity)
    for relative in metadata_inputs:
        if digest((ROOT/relative).read_bytes()) != source_inputs['system:'+relative]:
            raise ValueError('Build metadata changed during qualification: '+relative)
    if git(ROOT, 'rev-parse', 'HEAD').decode().strip() != source_commit:
        raise ValueError('System source commit changed during qualification; rebuild settled source')
    layout = inspect_elf(elf)
    (out/'elf-layout.json').write_text(json.dumps(layout, indent=2)+'\n')
    (out/'app_store.json').write_text(json.dumps(manifest(version), indent=2)+'\n')
    licenses = out/'licenses'
    licenses.mkdir()
    shutil.copyfile(ROOT/'LICENSE', licenses/'System-Apps-LICENSE.txt')
    missing_root_licenses = []
    for owner in PINS:
        names = git(repos[owner], 'ls-tree', '-r', '--name-only', PINS[owner]).decode().splitlines()
        notices = [name for name in names if Path(name).name.upper().startswith(('LICENSE', 'COPYING', 'NOTICE'))
                   or name == 'THIRD_PARTY_NOTICES.md' or name.startswith('licenses/')]
        if not any('/' not in name and Path(name).name.upper().startswith(('LICENSE', 'COPYING'))
                   for name in names): missing_root_licenses.append(owner)
        for name in notices:
            destination = licenses/owner/name
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(pinned(repos, owner, name))
    for folder in ('fonts', 'settings_fonts', 'quick_fonts', 'paper_fonts', 'time'):
        source = ROOT/'lib/PortableApps'/folder
        notices = {path for pattern in ('*LICENSE*.txt', '*OFL*.txt', '*SOURCES.json', '*PROVENANCE.json')
                   for path in source.glob(pattern)}
        for notice in sorted(notices):
            destination = licenses/folder/notice.name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(notice, destination)
    for name in ('LICENSE.txt', 'PROVENANCE.md'):
        source = ROOT/'lib/RemoteFiles/vendor/tinydtls_sha2'/name
        if source.is_file():
            destination = licenses/'tinydtls_sha2'/name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, destination)
    record = {'schema': 1, 'purpose': 'standalone-development-software-qualification-only',
        'bundle': 'native-app-store-'+version+'-dev1', 'version': version,
        'app_store_profile': 'native-app-store', 'native_ui': True, 'native_quick': True,
        'native_alarm': True, 'apps_only_service': True, 'quick_atomic_preferences': True,
        'zero_entry_catalog': True, 'async_rf_cleanup': True,
        'repository_commit': source_commit,
        'working_tree_dirty': bool(git(ROOT, 'status', '--porcelain').strip()),
        'external_sources': {key: {'repository': REPOSITORIES[key], 'commit': pin} for key, pin in PINS.items()},
        **compiler_record,
        'compiler': subprocess.check_output([cc, '--version'], text=True).splitlines()[0],
        'compiler_sha256': digest(Path(cc).read_bytes()),
        'sha256': digest(data), 'size_bytes': len(data),
        'elf_layout_sha256': digest((out/'elf-layout.json').read_bytes()),
        'command_sha256': digest((out/'command.json').read_bytes()), 'imports': sorted(imports), 'exports': sorted(exports),
        'build_defines': [item for item in arguments if item.startswith('-D')],
        'source_sha256': source_inputs,
        'notice_sha256': {str(path.relative_to(out)): digest(path.read_bytes())
                          for path in sorted(licenses.rglob('*')) if path.is_file()},
        'required_capabilities': manifest(version)['requires'],
        'shared_preferences_namespace': 1, 'wifi_credentials_namespace': 6,
        'software_update_apps_instance': 0,
        'wifi_capability_instance': 15,
        'physical_instance_bindings': 'deployment-owned; net.wifi grant must resolve instance 15',
        'retained_recipe_source': {'repository': 'michaelrolphone-cmyk/RiscRTE-System-Apps',
                                   'commit': RECIPE_COMMIT, 'path': RECIPE},
        'rtc_policy': 'fixed-UTC+08-to-America-Denver', 'local_navigation': 'input.power-key@1',
        'sleep': 'generic capability bridge; complete native-sized RGB565 image and foreground stack',
        'software_target_validation': True, 'compiled_version_verified': True,
        'capability_source_selectors': CAPABILITY_SELECTORS, 'host_fixture_excluded': True,
        'retained_wave_app_store_reproduction': False, 'firmware_or_product_replacement': False,
        'hardware_verified': False, 'external_sources_without_root_license_notice': missing_root_licenses}
    (out/'app-store-build-record.json').write_text(json.dumps(record, indent=2)+'\n')
    print('Native App Store '+version+': 16-capability target ELF and immutable input closure passed')


def argument_parser():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in PINS:
        parser.add_argument('--'+name.replace('_', '-'), type=Path, required=True,
                            help='Local Git repository containing exact '+PINS[name])
    parser.add_argument('--output-dir', type=Path, required=True)
    return parser


if __name__ == '__main__':
    build(argument_parser().parse_args())

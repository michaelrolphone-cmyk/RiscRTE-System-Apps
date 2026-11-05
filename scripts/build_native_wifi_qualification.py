#!/usr/bin/env python3
"""Build the explicitly selected native Wi-Fi software-qualification app.

This composes current System-Apps with immutable generic capability bridges.
It does not reconstruct a historical product ELF or assemble/install firmware.
The ordinary portable Wi-Fi builder and its defaults remain independent.
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
APP_MANIFEST = 'Apps/native/wifi_settings.json'
VERSION_PROFILE = 'lib/PortableApps/profiles/native-wifi.json'
DEFINES = (
    'PORTABLE_QUICK_NATIVE', 'PORTABLE_ALARM_NATIVE',
    'PORTABLE_WIFI_NATIVE_UI', 'PORTABLE_NATIVE_CANVAS',
           'PORTABLE_NATIVE_CUSTODY_FENCE')
# This declaration covers the complete delivered feature recipe. Namespace
# bindings remain deployment configuration, with required IDs recorded below.
CAPABILITIES = (
    ('display.output', 1), ('input.touch.raw', 1), ('rtc.clock', 2),
    ('alarm.service', 1), ('telemetry.broadcast', 1), ('contexts.service', 1),
    ('input.power-key', 1), ('power.sleep', 1), ('display.power', 1),
    ('board.battery', 1), ('storage.key-value', 1), ('net.wifi', 1),
    ('bluetooth.hci', 1), ('motion.accel', 1), ('app.catalog', 1),
    ('storage.key-value.atomic', 1), ('storage.key-value.availability', 1),
    ('ui.text-input', 1),
)
CAPABILITY_SELECTORS = {
    'display.output': 'PORTABLE_WIFI_SETTINGS_APP',
    'input.touch.raw': 'PORTABLE_WIFI_SETTINGS_APP',
    'rtc.clock': 'PORTABLE_RTC_UTC8_DENVER',
    'alarm.service': 'PORTABLE_ALARM_CLIENT',
    'telemetry.broadcast': 'PORTABLE_BLE_BROADCAST',
    'contexts.service': 'PORTABLE_CONTEXTS_CLIENT',
    'input.power-key': 'PORTABLE_INPUT_NAVIGATION_LOCAL',
    'power.sleep': 'PORTABLE_APP_SLEEP_LOCAL',
    'display.power': 'PORTABLE_APP_SLEEP_LOCAL',
    'board.battery': 'PORTABLE_LOW_BATTERY',
    'storage.key-value': 'PORTABLE_CHECKED_STORAGE_RUNTIME',
    'net.wifi': 'PORTABLE_WIFI_SETTINGS_APP',
    'bluetooth.hci': 'PORTABLE_QUICK_RADIOS',
    'motion.accel': 'PORTABLE_MOTION_WAKE',
    'app.catalog': 'PORTABLE_RUNTIME_APP_CATALOG',
    'storage.key-value.atomic': 'PORTABLE_WIFI_ATOMIC_EXPERIMENTAL',
    'storage.key-value.availability': 'PORTABLE_SETTINGS_AVAILABILITY',
    'ui.text-input': 'PORTABLE_TEXT_INPUT_CLIENT',
}
REQUIRED_FEATURES = set(CAPABILITY_SELECTORS.values()) | set(DEFINES) | {
    'PORTABLE_NOVA_UI', 'PORTABLE_INPUT_NAVIGATION', 'PORTABLE_NAVIGATION_RETENTION',
    'PORTABLE_QUICK_ACTIONS', 'PORTABLE_QUICK_RADIOS', 'PORTABLE_ASYNC_RGB565',
    'PORTABLE_FORCE_FULL_FRAMES', 'WATCH_ALARM_SLEEP_RESUME',
    'PORTABLE_WIFI_PROFILES', 'PORTABLE_WIFI_REMEMBER_CONNECTED',
    'PORTABLE_QUICK_ATOMIC_PREFERENCES', 'PORTABLE_HOME_AUTOMATIC_PREFERENCES',
    'PORTABLE_ALARM_TERMINAL_RETENTION', 'PORTABLE_WATCH_COLOR_ONLY',
    'PORTABLE_WATCH_NATIVE_HEIGHT',
}
REQUIRED_VALUES = {
    'PORTABLE_WIFI_STORAGE_INSTANCE': '6', 'PORTABLE_WIFI_INSTANCE': '15',
    'WIFI_RETURN_APP': '"springboard.elf"', 'PORTABLE_TOUCH_ROTATION': '0',
}
REQUIRED_SOURCES = {'wifi_settings.c', 'adapter.c', 'portable_navigation.c',
                    'portable_sleep.c', 'quick_actions.c', 'quick_render.c',
                    'quick_session.c', 'quick_radios.c', 'sha2.c'}
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
    return {'type': 'application', 'id': 'wifi_settings', 'version': version,
            'architecture': 'xtensa-esp32s3', 'file_name': 'wifi_settings.elf',
            'entry': 'app_main', 'requires': [
                {'capability': cap, 'api': api} for cap, api in CAPABILITIES]}


def validate_recipe(arguments, version):
    selected = {item[2:].split('=', 1)[0] for item in arguments if item.startswith('-D')}
    missing = REQUIRED_FEATURES - selected
    if missing:
        raise ValueError('Complete native Wi-Fi recipe lacks: '+', '.join(sorted(missing)))
    for name in REQUIRED_FEATURES:
        occurrences = [item for item in arguments if item == '-D'+name or item.startswith('-D'+name+'=')]
        if occurrences != ['-D'+name]:
            raise ValueError('Complete native Wi-Fi selector is ambiguous: '+name)
    for name, value in {**REQUIRED_VALUES, 'PORTABLE_WIFI_VERSION': '"'+version+'"'}.items():
        if [item for item in arguments if item.startswith('-D'+name+'=')] != ['-D'+name+'='+value]:
            raise ValueError('Complete native Wi-Fi recipe value differs: '+name)
    if {Path(item).name for item in arguments if item.endswith('.c')} != REQUIRED_SOURCES:
        raise ValueError('Complete native Wi-Fi source selection changed')
    if any(item.startswith('-UPORTABLE_') for item in arguments):
        raise ValueError('Complete native Wi-Fi features cannot be undefined')


def compose_arguments(recipe_bytes, repos, out, include, profile, runtime_include, version):
    if digest(recipe_bytes) != RECIPE_SHA256:
        raise ValueError('Retained full-feature Wi-Fi recipe digest changed')
    original = json.loads(recipe_bytes)['wifi_settings']['command']
    if original[0] != '@cc@' or original.count('-o') != 1:
        raise ValueError('Retained Wi-Fi command shape changed')
    output_index = original.index('-o')
    replacements = {
        '@r0@/lib/PortableApps/include': str(include),
        '@r0@': str(ROOT), '@r1@/sdk/app': str(runtime_include),
        '@r2@/Apps': str(profile), '@r2@/lib/Alarm/include': str(profile),
        '@r2@/lib/Contexts/include': str(runtime_include), '@r3@': str(repos['bridges']),
        '@work@/target-basic/input-fixtures': str(profile),
        '@work@/target-more/fixed/wifi_settings/exports.map': str(out/'wifi_settings.map'),
    }
    arguments = []
    for index, item in enumerate(original[1:], 1):
        if index in (output_index, output_index+1):
            continue
        for name, value in replacements.items():
            item = item.replace(name, value)
        if '@' in item:
            raise ValueError('Unresolved retained Wi-Fi recipe input: '+item)
        if item.startswith('-DPORTABLE_WIFI_VERSION='):
            item = '-DPORTABLE_WIFI_VERSION="'+version+'"'
        arguments.append(item)
    arguments += ['-D'+name for name in DEFINES if '-D'+name not in arguments]
    arguments += ['-I'+str(profile), '-I'+str(runtime_include)]
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
        raise ValueError('Native Wi-Fi manifest differs from the complete versioned recipe')
    arguments = compose_arguments(recipe_bytes, repos, out, include, profile, runtime_include, version)
    sources = [Path(item) for item in arguments if item.endswith('.c')]
    preprocessing = []
    for index, item in enumerate(arguments):
        if item.startswith(('-D', '-I')) or item == '-include' or (index and arguments[index-1] == '-include'):
            preprocessing.append(item)
    cc = compiler_path()
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
        'system:Apps/wifi_atomic.inc', 'system:lib/PortableApps/src/wifi_native_view.inc',
        'system:lib/PortableApps/src/native_custody_adapter.inc',
        'system:lib/PortableApps/src/text_input_handoff.inc',
        'system:lib/PortableApps/src/quick_atomic.inc',
        'system:lib/PortableApps/src/home_settings_availability.inc',
        'bridges:apps/clock/portable_navigation.c', 'bridges:apps/clock/portable_sleep.c',
    }
    if not required_inputs <= source_inputs.keys():
        raise ValueError('Complete native Wi-Fi source closure is absent: '+repr(required_inputs-source_inputs.keys()))
    metadata_inputs = (VERSION_PROFILE, RECIPE, APP_MANIFEST,
                       'scripts/build_native_wifi_qualification.py', 'scripts/native_toolchain.py',
                       'scripts/report_native_settings_memory.py')
    for relative in metadata_inputs:
        source_inputs['system:'+relative] = digest((ROOT/relative).read_bytes())
    source_commit = git(ROOT, 'rev-parse', 'HEAD').decode().strip()
    (out/'wifi_settings.map').write_text('{ global: '+ '; '.join(sorted(EXPORTS))+'; local: *; };\n')
    elf = out/'wifi_settings.elf'
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
        raise ValueError('Selected Wi-Fi version is absent from target ELF')
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
    (out/'wifi_settings.json').write_text(json.dumps(manifest(version), indent=2)+'\n')
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
        'bundle': 'native-wifi-'+version+'-dev1', 'version': version,
        'wifi_profile': 'native-wifi', 'native_ui': True, 'native_quick': True, 'native_alarm': True, 'atomic_wifi_persistence': True, 'settings_availability': True,
        'wifi_profiles': True, 'shared_text_input': True, 'async_rf_cleanup': True,
        'repository_commit': source_commit,
        'working_tree_dirty': bool(git(ROOT, 'status', '--porcelain').strip()),
        'external_sources': {key: {'repository': REPOSITORIES[key], 'commit': pin} for key, pin in PINS.items()},
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
        'wifi_capability_instance': 15,
        'physical_instance_bindings': 'deployment-owned; net.wifi grant must resolve instance 15',
        'retained_recipe_source': {'repository': 'michaelrolphone-cmyk/RiscRTE-System-Apps',
                                   'commit': RECIPE_COMMIT, 'path': RECIPE},
        'rtc_policy': 'fixed-UTC+08-to-America-Denver', 'local_navigation': 'input.power-key@1',
        'sleep': 'generic capability bridge; complete native-sized RGB565 image and foreground stack',
        'software_target_validation': True, 'compiled_version_verified': True,
        'capability_source_selectors': CAPABILITY_SELECTORS, 'host_fixture_excluded': True,
        'retained_wave_wifi_1_1_34_reproduction': False, 'firmware_or_product_replacement': False,
        'hardware_verified': False, 'external_sources_without_root_license_notice': missing_root_licenses}
    (out/'wifi-build-record.json').write_text(json.dumps(record, indent=2)+'\n')
    print('Native Wi-Fi '+version+': 18-capability target ELF and immutable input closure passed')


def argument_parser():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in PINS:
        parser.add_argument('--'+name.replace('_', '-'), type=Path, required=True,
                            help='Local Git repository containing exact '+PINS[name])
    parser.add_argument('--output-dir', type=Path, required=True)
    return parser


if __name__ == '__main__':
    build(argument_parser().parse_args())

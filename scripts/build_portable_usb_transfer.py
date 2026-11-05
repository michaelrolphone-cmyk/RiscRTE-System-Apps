#!/usr/bin/env python3
"""Build USB SD transfer; --native-ui is an opt-in, source-only RGB profile."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

from native_toolchain import compiler_path as resolve_compiler
import portable_quick_build
import portable_alarm_build

ROOT = Path(__file__).resolve().parents[1]
MSC_HEADER = 'RiscUsbDeviceMscV1.h'
MSC_SHA256 = 'd55a9340c3ff64e8679c4d9332812f073c30b8aa08ae3e3fb04f9010cf517ed6'
MSC_READER_COMMIT = '322fc9c0fbe6713a9fc0f3175a19550970682eaa'
NATIVE_FLAGS = ['-DPORTABLE_USB_TRANSFER_NATIVE_UI', '-DPORTABLE_NOVA_UI',
                '-DPORTABLE_NATIVE_CANVAS', '-DPORTABLE_NOVA_TEXT_FACES_012']
FLAGS = ['-DPORTABLE_USB_TRANSFER_APP', '-DPORTABLE_APP_LAUNCH_GUARD',
         '-DPORTABLE_APP_OWNS_TOUCH_CHROME', '-DPORTABLE_INPUT_NAVIGATION',
         '-DPORTABLE_DISPLAY_ROTATION=90', '-DPORTABLE_HOME_APP="default.elf"',
         '-DPORTABLE_RETURN_APP="springboard.elf"']
NATIVE_SOURCES = (
    'Apps/usb_sd_transfer.c', 'Apps/UsbTransferNative.h', 'Apps/UsbTransferLayout.h',
    'Apps/usb_sd_transfer_native.inc', 'Apps/PaperPresentation.h',
    'lib/PortableApps/include/PortableUsbTransfer.h',
    'lib/PortableApps/include/PortableNativeCanvas.h',
    'lib/PortableApps/include/PortableNovaUi.h',
    'lib/PortableApps/include/PortableDisplayGeometry.h',
    'lib/PortableApps/src/adapter.c', 'lib/PortableApps/src/paper.inc',
    'lib/PortableApps/src/nova.inc', 'lib/PortableApps/src/nova_ui.inc',
    'lib/PortableApps/src/nova_native.inc',
    'lib/PortableApps/settings_fonts/text-faces-012.inc',
    'scripts/build_portable_usb_transfer.py', 'scripts/native_toolchain.py',
    'scripts/portable_quick_build.py', 'scripts/portable_alarm_build.py',
    'scripts/portable_idle_build.py')
FORBIDDEN_OPTIONS = ('quick_actions', 'quick_native', 'quick_radios', 'quick_usb_transfer',
    'paper_transitions', 'x4_idle_source', 'x4_idle_sdk', 'x4_idle_runtime_sdk',
    'resident_legacy_handoff', 'frontlight_tone', 'frontlight_tone_sdk',
    'crash_report_sd', 'display_settled_sdk')
ALLOWED_IMPORTS = {'risc_runtime_get_api', 'memcpy', 'memset', 'memcmp', 'strcmp',
                   'strlen', 'snprintf', 'malloc', 'free', 'strcpy'}
PRIVATE_QUERY = 'portable_usb_transfer_native_available'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def argument_parser():
    parser = argparse.ArgumentParser(description=__doc__)
    portable_quick_build.options(parser)
    portable_alarm_build.options(parser)
    parser.add_argument('--native-ui', action='store_true',
        help='Opt-in source-only native RGB geometry; no released version or product binding')
    parser.add_argument('--touch-only', action='store_true',
        help='Use on-screen controls without an input.navigation provider')
    parser.add_argument('--msc-sdk', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, default=ROOT/'dist/portable/usb-transfer')
    return parser


def validate_selection(args, root=ROOT):
    """Fail before output or compiler use; never silently ignore a shared option."""
    if getattr(args, 'resident_shell_host', False):
        raise ValueError('USB transfer is a foreground client, never a resident host')
    for name in FORBIDDEN_OPTIONS:
        if getattr(args, name, None):
            raise ValueError('--' + name.replace('_', '-') + ' is not supported by USB transfer')
    if getattr(args, 'crash_report_spool_namespace', 62) != 62:
        raise ValueError('--crash-report-spool-namespace is not supported by USB transfer')
    client = getattr(args, 'resident_shell_client', False)
    for name in ('resident_runtime_sdk', 'resident_policy', 'tagged_alarm_utilities'):
        if getattr(args, name, None) and not client:
            raise ValueError('--' + name.replace('_', '-') + ' requires --resident-shell-client')
    if client and not getattr(args, 'tagged_alarm_utilities', None):
        raise ValueError('Resident USB requires the canonical tagged alarm SDK')
    native = getattr(args, 'native_ui', False)
    if native and client:
        raise ValueError('--native-ui is qualified only for standalone foreground USB; resident native selection is not supported')
    if getattr(args, 'alarm_native', False) and not (native and client):
        raise ValueError('--alarm-native requires --native-ui and --resident-shell-client')
    flags = list(FLAGS)
    if getattr(args, 'touch_only', False):
        flags.remove('-DPORTABLE_INPUT_NAVIGATION')
    if getattr(args, 'home_app', None):
        flags.remove('-DPORTABLE_HOME_APP="default.elf"')
    if native:
        missing = [name for name in NATIVE_SOURCES if not (root/name).is_file()]
        if missing:
            raise ValueError('--native-ui requires the USB native implementation: ' + ', '.join(missing))
        flags += NATIVE_FLAGS
    return flags


def validate_sdk(header, native):
    if not header.is_file():
        raise ValueError('Canonical USB MSC SDK header is required')
    if native and sha(header) != MSC_SHA256:
        raise ValueError('--native-ui requires the pinned complete MSC diagnostics-v2 SDK')


def native_receipt(source_hashes):
    return {'profile': 'usb-transfer-native-v1', 'selected': True,
        'coordinates': 'capability-pixels', 'glyphs': 'native-font-metrics',
        'paper_path': 'unchanged', 'minimum_rgb': [160, 160],
        'source_only': True, 'hardware_verified': False, 'product_bound': False,
        'released_artifact': False, 'version_claim': False, 'baseline_version': '0.1.11',
        'grants_added': False,
        'source_sha256': {name: source_hashes[name] for name in NATIVE_SOURCES}}


def dependencies(cc, flags, includes, sources):
    """Bind the actual preprocessed non-system dependency closure for each TU."""
    found = set()
    for source in sources:
        output = subprocess.check_output([cc, '-std=c11', *flags,
            '-I'+str(includes), '-I'+str(ROOT/'lib/NativeApps/include'),
            '-MM', '-MT', 'usb_source', str(source)], text=True)
        found.update(Path(name).resolve() for name in
            shlex.split(output.replace('\\\n', '').split(':', 1)[1]))
    return found


def build(args):
    flags = validate_selection(args)
    native = getattr(args, 'native_ui', False)
    header = args.msc_sdk.resolve()/MSC_HEADER
    validate_sdk(header, native)
    manifest = json.loads((ROOT/'Apps/native/usb_sd_transfer.json').read_text())
    if native and manifest['version'] != '0.1.11':
        raise ValueError('USB native source profile requires explicit requalification after a baseline version change')
    cc = resolve_compiler()
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    includes = out/'include'
    # Fresh staged include trees prevent a previous optional SDK leaking into a new build.
    if includes.exists():
        shutil.rmtree(includes)
    shutil.copytree(ROOT/'lib/PortableApps/include', includes)
    shutil.copyfile(header, includes/header.name)
    args.alarm_client = args.resident_shell_client
    if args.resident_shell_client:
        portable_alarm_build.stage(args, argument_parser(), out, includes)
        flags += ['-DPORTABLE_NATIVE_CUSTODY_FENCE', '-DPORTABLE_ALARM_TERMINAL_RETENTION',
                  '-DPORTABLE_ALARM_CLIENT', '-DALARM_SERVICE_TAGGED_V2']
    selected, extra_sources = portable_quick_build.configure(args, argument_parser(), ROOT, out, includes)
    if extra_sources:
        raise ValueError('USB transfer cannot select Quick renderer or sleep sources')
    flags += selected
    exports = portable_quick_build.exports(args, {'app_main', 'app_module_init', 'app_module_fini'})
    mapping = out/'usb_sd_transfer.map'
    mapping.write_text('{ global: '+'; '.join(sorted(exports))+'; local: *; };\n')
    catalog = out/'catalog.c'
    catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
    elf = out/'usb_sd_transfer.elf'
    sources = [ROOT/'Apps/usb_sd_transfer.c', ROOT/'lib/PortableApps/src/adapter.c', catalog]
    dependency_files = dependencies(cc, flags, includes, sources)
    files = {ROOT/'Apps/usb_sd_transfer.c', ROOT/'Apps/native/usb_sd_transfer.json',
             ROOT/'Apps/PaperPresentation.h', Path(__file__).resolve()}
    files.update(p for p in (ROOT/'lib/PortableApps').rglob('*') if p.is_file())
    files.update(ROOT/name for name in NATIVE_SOURCES if native or name.startswith('scripts/'))
    files.update(p for p in dependency_files if p.is_relative_to(ROOT) and not p.is_relative_to(out))
    source_hashes = {str(p.relative_to(ROOT)): sha(p) for p in sorted(files)}
    input_hashes = {p: sha(p) for p in dependency_files | files | {header}}
    command = [cc, '-std=c11', '-Os', '-fPIC', '-mtext-section-literals', '-mlongcalls',
        '-fvisibility=hidden', '-ffreestanding', '-fno-builtin', '-nostdlib', '-nostartfiles',
        '-shared', '-Wl,--no-relax', '-Wl,--hash-style=sysv', '-Wl,--version-script='+str(mapping),
        '-Wall', '-Wextra', '-Werror', *flags, '-I'+str(includes),
        '-I'+str(ROOT/'lib/NativeApps/include'), *map(str, sources), '-lgcc', '-o', str(elf)]
    subprocess.run(command, check=True)
    nm = cc.removesuffix('gcc')+'nm'
    symbols = subprocess.check_output([nm, '-D', str(elf)], text=True)
    imports = {x.split()[-1] for x in symbols.splitlines() if ' U ' in ' '+x}
    actual = {x.split()[-1] for x in subprocess.check_output(
        [nm, '-D', '--defined-only', str(elf)], text=True).splitlines() if x.split()}
    if not imports <= ALLOWED_IMPORTS or actual != exports:
        raise ValueError(f'Unexpected ELF ABI: {imports-ALLOWED_IMPORTS}, {actual}')
    local_symbols = subprocess.check_output([nm, '--defined-only', str(elf)], text=True)
    query = [x.split() for x in local_symbols.splitlines() if x.split()[-1] == PRIVATE_QUERY]
    if PRIVATE_QUERY in imports or PRIVATE_QUERY in actual or bool(query) != native:
        raise ValueError('USB native private query must exist only in selected builds and never in the dynamic ABI')
    if native and (len(query) != 1 or query[0][-2] != 't'):
        raise ValueError('USB native query must be a local text symbol')
    data = elf.read_bytes()
    if data[:7] != b'\x7fELF\x01\x01\x01' or data[16:20] != b'\x03\x00\x5e\x00':
        raise ValueError('Expected Xtensa ELF32 ET_DYN')
    validator = out/'validate-elf'
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
        '-I'+str(ROOT/'test/native_apps/stubs'), '-I'+str(ROOT/'lib/elf_loader/include'),
        str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'),
        str(ROOT/'test/native_apps/validate_test.c'), '-o', str(validator)], check=True)
    subprocess.run([str(validator), str(elf)], check=True)
    manifest['version'] = portable_quick_build.version(args, 'usb_sd_transfer', manifest['version'])
    if args.touch_only:
        manifest['requires'] = [r for r in manifest['requires'] if r['capability'] != 'input.navigation']
    if args.resident_shell_client:
        manifest['requires'].append({'capability': 'alarm.service', 'api': 2})
    (out/'usb_sd_transfer.json').write_text(json.dumps(manifest, indent=2)+'\n')
    licenses = out/'licenses'
    licenses.mkdir(exist_ok=True)
    shutil.copyfile(ROOT/'LICENSE', licenses/'System-Apps-LICENSE.txt')
    for group in ('settings_fonts', 'fonts', 'paper_fonts'):
        target = licenses/group
        target.mkdir(exist_ok=True)
        for path in (ROOT/'lib/PortableApps'/group).glob('LICENSE*'):
            shutil.copyfile(path, target/path.name)
        source = ROOT/'lib/PortableApps'/group/'SOURCES.json'
        if source.exists():
            shutil.copyfile(source, target/source.name)
    changed = [str(path) for path, digest in input_hashes.items() if not path.is_file() or sha(path) != digest]
    if changed:
        raise ValueError('Build inputs changed during qualification: '+', '.join(changed))
    libgcc = Path(subprocess.check_output([cc, '-print-libgcc-file-name'], text=True).strip())
    record = {'schema': 2, 'version': manifest['version'],
        'repository_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        'working_tree_dirty': bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True)),
        'compiler': subprocess.check_output([cc, '--version'], text=True).splitlines()[0],
        'compiler_sha256': sha(Path(cc)), 'libgcc_sha256': sha(libgcc),
        'defines': flags, 'imports': sorted(imports), 'exports': sorted(exports),
        'sha256': sha(elf), 'size_bytes': len(data), 'source_sha256': source_hashes,
        'compiled_source_sha256': {str(p.relative_to(ROOT)): sha(p) for p in sorted(dependency_files)
                                  if p.is_relative_to(ROOT) and not p.is_relative_to(out)},
        'compiled_sdk_sha256': {str(p.relative_to(includes)): sha(p) for p in sorted(dependency_files)
                               if p.is_relative_to(includes)},
        'compiled_external_sha256': {str(p): sha(p) for p in sorted(dependency_files)
                                   if not p.is_relative_to(ROOT) and not p.is_relative_to(out)},
        'sdk_sha256': {header.name: sha(header)},
        'loader_validation': {'passed': True, 'negative_controls': True,
            'format': 'ELF32 little-endian Xtensa ET_DYN'},
        'private_query': {'symbol': PRIVATE_QUERY, 'present': native, 'dynamic_export': False},
        'capability': {'name': 'usb.device.msc', 'api': 1, 'instance_id': 0},
        'poll_interval_ms': 2, 'connected_poll_interval_ms': 1, 'usb_role': 'device',
        'sleep': False, 'navigation_requires_release': True,
        'configured_stop_requires_cable_confirmation': True,
        'sd_preparation': {'extension_tag': '0x554d5031', 'extension_version': 1,
            'explicit_step': True, 'settled_screen_required': True,
            'max_steps_per_input_poll': 1, 'wait_poll_prepares': False}}
    if sha(header) == MSC_SHA256:
        record['sdk_origin'] = {'repository': 'michaelrolphone-cmyk/T5S3-Reader', 'commit': MSC_READER_COMMIT,
                                'path': 'sdk/driver/'+MSC_HEADER, 'sha256': MSC_SHA256}
    if native:
        record['native_ui'] = native_receipt(source_hashes)
    portable_quick_build.record(args, record)
    artifacts = [elf, out/'usb_sd_transfer.json', mapping, catalog]
    artifacts += [p for p in licenses.rglob('*') if p.is_file()]
    record['artifact_sha256'] = {str(p.relative_to(out)): sha(p) for p in sorted(artifacts)}
    (out/'usb_sd_transfer-build-record.json').write_text(json.dumps(record, indent=2)+'\n')
    print('USB SD Transfer: strict Xtensa ELF, private selector, loader and exact ABI passed')
    return record


if __name__ == '__main__':
    parser = argument_parser()
    try:
        build(parser.parse_args())
    except (ValueError, FileNotFoundError) as error:
        parser.error(str(error))

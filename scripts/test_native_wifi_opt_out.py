#!/usr/bin/env python3
"""Prove six native-UI-off Wi-Fi ELFs match the immutable legacy baseline.

Uses one selected compiler for both source trees. No product builders, providers,
devices or release assets are involved. Successful ELFs are reproducible and are
removed after their commands and hashes are recorded; mismatches are retained.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BASELINE = 'fd9a028c5d38eb517c1328b869684c4330a5d4d9'
PROFILES = {
    'default': [],
    'nova': ['PORTABLE_NOVA_UI'],
    'nova180': ['PORTABLE_NOVA_UI', 'PORTABLE_TOUCH_ROTATION=180'],
    'native-height': ['PORTABLE_NOVA_UI', 'PORTABLE_WATCH_NATIVE_HEIGHT', 'PORTABLE_ASYNC_RGB565'],
    'paper-x4': ['PORTABLE_NOVA_UI', 'PORTABLE_DISPLAY_ROTATION=90',
                 'PORTABLE_RTC_WALL_TIME', 'PORTABLE_INPUT_NAVIGATION'],
    'atomic': ['PORTABLE_NOVA_UI', 'PORTABLE_WATCH_NATIVE_HEIGHT', 'PORTABLE_ASYNC_RGB565',
               'PORTABLE_WIFI_PROFILES', 'PORTABLE_WIFI_REMEMBER_CONNECTED',
               'PORTABLE_TEXT_INPUT_CLIENT', 'PORTABLE_WIFI_ATOMIC_EXPERIMENTAL',
               'PORTABLE_SETTINGS_AVAILABILITY',
               'PORTABLE_NATIVE_CUSTODY_FENCE'],
}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT), *args])


def source_hashes(root):
    return {str(path.relative_to(root)): sha(path)
            for directory in ('Apps', 'lib')
            for path in sorted((root / directory).rglob('*')) if path.is_file()}


def build(cc, root, profile, definitions, catalog, mapping, output):
    version = json.loads((root/'Apps/wifi_settings.json').read_text())['version']
    sources = [root/'Apps/wifi_settings.c', root/'lib/PortableApps/src/adapter.c', catalog]
    if profile == 'atomic':
        sources.append(root/'lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c')
    command = [str(cc), '-std=c11', '-Os', '-fPIC', '-mtext-section-literals',
               '-mlongcalls', '-fvisibility=hidden', '-ffreestanding', '-fno-builtin',
               '-Wall', '-Wextra', '-Werror', '-DPORTABLE_WIFI_SETTINGS_APP',
               '-DPORTABLE_WIFI_STORAGE_INSTANCE=6', '-UPORTABLE_WIFI_NATIVE_UI',
               '-DPORTABLE_WIFI_VERSION="' + version + '"',
               *['-D' + value for value in definitions],
               '-I' + str(root / 'lib/PortableApps/include'),
               '-I' + str(root / 'lib/NativeApps/include'),
               '-I' + str(root / 'lib/RemoteFiles/vendor/tinydtls_sha2'),
               '-nostdlib', '-nostartfiles', '-shared', '-Wl,--hash-style=sysv',
               '-Wl,--version-script=' + str(mapping), *map(str, sources),
               '-o', str(output)]
    subprocess.run(command, check=True, timeout=120)
    return {'command': command, 'sha256': sha(output), 'bytes': output.stat().st_size}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--xtensa-cc', required=True, help='Selected Xtensa compiler executable')
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    from native_toolchain import compiler_path
    cc, out = Path(compiler_path(args.xtensa_cc)), args.output.resolve()
    if out.exists() and any(out.iterdir()):
        parser.error('Use a new or empty output directory; stale witnesses are refused')
    out.mkdir(parents=True, exist_ok=True)
    catalog, mapping = out / 'catalog.c', out / 'wifi_settings.map'
    catalog.write_text('#include "PortableApps.h"\n'
                       'const t5_app_manifest_t portable_catalog[]={{.compatible=false}};\n'
                       'const unsigned portable_catalog_count=0;\n')
    mapping.write_text('{ global: app_main; app_module_init; app_module_fini; local: *; };\n')
    receipt = {
        'baseline_commit': BASELINE,
        'candidate_head_commit': git('rev-parse', 'HEAD').decode().strip(),
        'candidate_dirty_paths': git('status', '--short', '--', 'Apps', 'lib').decode().splitlines(),
        'feature': 'PORTABLE_WIFI_NATIVE_UI', 'feature_enabled': False,
        'compiler': subprocess.check_output([str(cc), '--version'], text=True).splitlines()[0],
        'compiler_path': str(cc), 'compiler_sha256': sha(cc),
        'runner_sha256': sha(Path(__file__)),
        'generated_input_sha256': {path.name: sha(path) for path in (catalog, mapping)},
        'profile_definitions': PROFILES, 'profiles': {}, 'hardware_tested': False,
        'status': 'RUNNING',
    }
    elfs = []
    try:
        with tempfile.TemporaryDirectory(prefix='native-wifi-opt-out-', dir=out) as temporary:
            stage = Path(temporary)
            candidate, baseline = stage / 'candidate', stage / 'baseline'
            baseline.mkdir()
            for directory in ('Apps', 'lib'):
                shutil.copytree(ROOT / directory, candidate / directory)
            current_hashes = source_hashes(candidate)
            if current_hashes != source_hashes(ROOT):
                raise RuntimeError('Production source changed during snapshot')
            with tarfile.open(fileobj=io.BytesIO(git('archive', BASELINE, 'Apps', 'lib'))) as archive:
                archive.extractall(baseline, filter='data')
            hashes = {'candidate': current_hashes, 'baseline': source_hashes(baseline)}
            (out / 'source-hashes.json').write_text(json.dumps(hashes, indent=2) + '\n')
            receipt['source_hashes'] = 'source-hashes.json'
            receipt['source_hashes_sha256'] = sha(out / 'source-hashes.json')
            for profile, definitions in PROFILES.items():
                row = {}
                pair = []
                for label, root in (('base', baseline), ('candidate', candidate)):
                    elf = out / (profile + '-' + label + '.elf')
                    elfs.append(elf)
                    pair.append(elf)
                    row[label] = build(cc, root, profile, definitions, catalog, mapping, elf)
                row['identical_bytes'] = pair[0].read_bytes() == pair[1].read_bytes()
                receipt['profiles'][profile] = row
                print(profile + ': ' + ('PASS' if row['identical_bytes'] else 'DIFFERENT') +
                      ' (' + str(row['candidate']['bytes']) + ' bytes)', flush=True)
            if hashes['baseline'] != source_hashes(baseline):
                raise RuntimeError('Immutable baseline snapshot changed during qualification')
            if current_hashes != source_hashes(candidate):
                raise RuntimeError('Frozen candidate source changed during qualification')
            receipt['candidate_matches_checkout_at_completion'] = current_hashes == source_hashes(ROOT)
            if not receipt['candidate_matches_checkout_at_completion']:
                raise RuntimeError('Production source changed during qualification; rerun the witness')
            if not all(row['identical_bytes'] for row in receipt['profiles'].values()):
                raise RuntimeError('Default-off Wi-Fi ELF bytes differ from baseline')
            receipt['status'] = 'PASS'
    except Exception as error:
        receipt['status'] = 'FAIL'
        receipt['error'] = str(error)
        raise
    finally:
        (out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    for elf in elfs:
        elf.unlink()
    print('Six native-UI-off Wi-Fi profiles match the immutable baseline byte-for-byte.')


if __name__ == '__main__':
    main()

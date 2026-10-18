#!/usr/bin/env python3
"""Prove twelve native-UI-off App Store and firmware updater ELFs match the immutable legacy baseline.

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
BASELINE = '02f65827d524ec7b1bd6a5d0576a9d731cc589d5'
BASELINE_TREE = 'e32ad8d0feeec9280569b7dccb5997c3738e2669'
PROFILES = {
    'default': [],
    'nova': ['PORTABLE_NOVA_UI'],
    'nova180': ['PORTABLE_NOVA_UI', 'PORTABLE_TOUCH_ROTATION=180'],
    'native-height': ['PORTABLE_NOVA_UI', 'PORTABLE_WATCH_NATIVE_HEIGHT', 'PORTABLE_ASYNC_RGB565'],
    'paper-x4': ['PORTABLE_NOVA_UI', 'PORTABLE_DISPLAY_ROTATION=90',
                 'PORTABLE_RTC_WALL_TIME', 'PORTABLE_INPUT_NAVIGATION'],
    'custody': ['PORTABLE_NOVA_UI', 'PORTABLE_WATCH_NATIVE_HEIGHT', 'PORTABLE_ASYNC_RGB565',
                       'PORTABLE_NATIVE_CANVAS', 'PORTABLE_NATIVE_CUSTODY_FENCE'],
}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT), *args])


def source_hashes(root):
    return {str(path.relative_to(root)): sha(path)
            for directory in ('Apps', 'lib')
            for path in sorted((root / directory).rglob('*')) if path.is_file()}


def build(cc, root, profile, definitions, catalog, mapping, output, app):
    sources = [root/('Apps/'+app+'.c'), root/'lib/PortableApps/src/adapter.c', catalog]
    command = [str(cc), '-std=c11', '-Os', '-fPIC', '-mtext-section-literals',
               '-mlongcalls', '-fvisibility=hidden', '-ffreestanding', '-fno-builtin',
               '-Wall', '-Wextra', '-Werror', '-Wno-misleading-indentation',
               '-DPORTABLE_UPDATE_APP', '-DPORTABLE_UPDATE_FIRMWARE='+str(int(app=='ota_update')),
               '-DPORTABLE_WIFI_INSTANCE=15', '-DPORTABLE_UPDATE_RTC_UTC_OFFSET_SECONDS=28800',
               '-UPORTABLE_UPDATE_NATIVE_UI', *['-D' + value for value in definitions],
               '-I' + str(root / 'lib/PortableApps/include'),
               '-I' + str(root / 'lib/NativeApps/include'),
               '-nostdlib', '-nostartfiles', '-shared', '-Wl,--hash-style=sysv',
               '-Wl,--version-script=' + str(mapping), *map(str, sources),
               '-o', str(output)]
    result = subprocess.run(command, capture_output=True, text=True, timeout=120)
    output.with_suffix('.stdout').write_text(result.stdout)
    output.with_suffix('.stderr').write_text(result.stderr)
    result.check_returncode()
    data = output.read_bytes()
    if data[:7] != b'\x7fELF\x01\x01\x01' or data[16:20] != b'\x03\x00\x5e\x00':
        raise ValueError('Expected target-correct little-endian Xtensa ELF')
    return {'command': command, 'sha256': sha(output), 'bytes': output.stat().st_size}


def materialize_baseline(destination, baseline_dir):
    repo = baseline_dir.resolve() if baseline_dir else ROOT
    commit = 'HEAD' if baseline_dir else BASELINE
    tree = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', commit+'^{tree}'], text=True).strip()
    if tree != BASELINE_TREE:
        raise RuntimeError('Immutable baseline tree differs from System '+BASELINE)
    # Sparse replicas may contain an equivalent local commit without the public
    # parent object graph. Only the complete tree identity can authorize that alias.
    with tarfile.open(fileobj=io.BytesIO(subprocess.check_output(
            ['git', '-C', str(repo), 'archive', commit, 'Apps', 'lib']))) as archive:
        archive.extractall(destination, filter='data')
    return subprocess.check_output(['git', '-C', str(repo), 'rev-parse', commit], text=True).strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--xtensa-cc', required=True, help='Selected Xtensa compiler executable')
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--baseline-dir', type=Path, help='Optional immutable tree-equivalent local baseline repository')
    args = parser.parse_args()
    from native_toolchain import compiler_path
    cc, out = Path(compiler_path(args.xtensa_cc)), args.output.resolve()
    if out.exists() and any(out.iterdir()):
        parser.error('Use a new or empty output directory; stale witnesses are refused')
    out.mkdir(parents=True, exist_ok=True)
    catalog, mapping = out / 'catalog.c', out / 'app_store.map'
    catalog.write_text('#include "PortableApps.h"\n'
                       'const t5_app_manifest_t portable_catalog[]={{.compatible=false}};\n'
                       'const unsigned portable_catalog_count=0;\n')
    mapping.write_text('{ global: app_main; app_module_init; app_module_fini; local: *; };\n')
    receipt = {
        'baseline_commit': BASELINE, 'baseline_tree': BASELINE_TREE,
        'candidate_head_commit': git('rev-parse', 'HEAD').decode().strip(),
        'candidate_dirty_paths': git('status', '--short', '--', 'Apps', 'lib').decode().splitlines(),
        'feature': 'PORTABLE_UPDATE_NATIVE_UI', 'feature_enabled': False,
        'compiler': subprocess.check_output([str(cc), '--version'], text=True).splitlines()[0],
        'compiler_path': str(cc), 'compiler_sha256': sha(cc),
        'runner_sha256': sha(Path(__file__)),
        'generated_input_sha256': {path.name: sha(path) for path in (catalog, mapping)},
        'profile_definitions': PROFILES, 'profiles': {}, 'hardware_tested': False,
        'status': 'RUNNING',
    }
    elfs = []
    try:
        with tempfile.TemporaryDirectory(prefix='native-app-store-opt-out-', dir=out) as temporary:
            stage = Path(temporary)
            candidate, baseline = stage / 'candidate', stage / 'baseline'
            baseline.mkdir()
            for directory in ('Apps', 'lib'):
                shutil.copytree(ROOT / directory, candidate / directory)
            current_hashes = source_hashes(candidate)
            if current_hashes != source_hashes(ROOT):
                raise RuntimeError('Production source changed during snapshot')
            receipt['materialized_baseline_commit'] = materialize_baseline(baseline, args.baseline_dir)
            hashes = {'candidate': current_hashes, 'baseline': source_hashes(baseline)}
            (out / 'source-hashes.json').write_text(json.dumps(hashes, indent=2) + '\n')
            receipt['source_hashes'] = 'source-hashes.json'
            receipt['source_hashes_sha256'] = sha(out / 'source-hashes.json')
            for app, profile, definitions in ((app, p, d) for app in ('app_store', 'ota_update') for p, d in PROFILES.items()):
                profile = app+'-'+profile
                row = {}
                pair = []
                for label, root in (('base', baseline), ('candidate', candidate)):
                    elf = out / (profile + '-' + label + '.elf')
                    elfs.append(elf)
                    pair.append(elf)
                    row[label] = build(cc, root, profile, definitions, catalog, mapping, elf, app)
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
                raise RuntimeError('Default-off App Store and firmware updater ELF bytes differ from baseline')
            receipt['status'] = 'PASS'
    except Exception as error:
        receipt['status'] = 'FAIL'
        receipt['error'] = str(error)
        raise
    finally:
        (out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    for elf in elfs:
        elf.unlink()
    print('Twelve native-UI-off App Store and firmware updater profiles match the immutable baseline byte-for-byte.')


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Build/validate repository-owned apps; emit development evidence, never release."""
import argparse
import hashlib
import json
import os
import pathlib
import shutil
import subprocess
import sys
from app_manifest import validate_manifest
from check_baseline import check_sdk, git_blob
from package_integrity import stamp_app_manifest

ROOT = pathlib.Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--id', help='Build one inventory app ID')
    args = parser.parse_args()
    sdk = check_sdk()
    inventory = json.loads((ROOT / 'system-apps-manifest.json').read_text())['apps']
    apps = [app for app in inventory if not args.id or app['id'] == args.id]
    if not apps:
        parser.error('No matching app in the migration inventory')
    # Clear only our generated output tree so removed apps cannot leak into artifacts.
    output = ROOT / 'dist/apps'
    if output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True)
    validator = ROOT / 'build/validate-elf'
    validator.parent.mkdir(exist_ok=True)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-I' + str(ROOT / 'test/native_apps/stubs'),
                    '-I' + str(ROOT / 'lib/elf_loader/include'),
                    str(ROOT / 'lib/elf_loader/src/esp_elf_validate.c'),
                    str(ROOT / 'test/native_apps/validate_test.c'), '-o', str(validator)], check=True)
    records = []
    for app in apps:
        source = ROOT / app['source_path']
        elf = output / app['file_name']
        validate_manifest(source, elf)
        subprocess.run([sys.executable, str(ROOT / 'scripts/build_native_app.py'), str(source),
                        '--output', str(elf), '--require-manifest'], check=True, timeout=120)
        subprocess.run([str(validator), str(elf)], check=True, timeout=60)
        manifest = json.loads(elf.with_suffix('.json').read_text())
        manifest = stamp_app_manifest(manifest, elf)
        if manifest['version'] != app['version']:
            raise ValueError('Inventory/manifest version mismatch: ' + app['id'])
        elf.with_suffix('.json').write_text(json.dumps(manifest, indent=2) + '\n')
        records.append({'id': app['id'], 'version': manifest['version'], 'file_name': elf.name,
                        'sha256': manifest['sha256'], 'size_bytes': manifest['size_bytes'],
                        'source_blob': git_blob(source.read_bytes()),
                        'manifest_blob': git_blob(source.with_suffix('.json').read_bytes())})
    cc = os.environ.get('NATIVE_APP_CC') or shutil.which('xtensa-esp32s3-elf-gcc')
    if not cc:
        cc = str(pathlib.Path(os.environ.get('PLATFORMIO_CORE_DIR', pathlib.Path.home() / '.platformio')) / 'packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc')
    report = {'schema': 1, 'purpose': 'development-build-evidence-not-install-catalog',
              'sdk_commit': sdk['source_commit'],
              'repository_commit': subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip(),
              'working_tree_dirty': bool(subprocess.check_output(['git', '-C', str(ROOT), 'status', '--porcelain'], text=True).strip()),
              'compiler': subprocess.check_output([cc, '--version'], text=True).splitlines()[0],
              'apps': records}
    (output / 'build-evidence.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'Validated {len(records)} application ELFs and digest/version sidecars')


if __name__ == '__main__':
    main()

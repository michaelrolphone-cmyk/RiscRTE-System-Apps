#!/usr/bin/env python3
"""Strict Xtensa USB source qualification, reproducibility and pristine opt-out.

Build products remain in temporary directories. Retained output is evidence,
not a product bundle or a new release/version claim.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

import build_portable_usb_transfer as usb
from native_toolchain import compiler_path

ROOT = Path(__file__).resolve().parents[1]
BASELINE = 'ff6151285ee41bdbeff75ebd31ee2621d524e899'


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--msc-sdk', type=Path, required=True)
    parser.add_argument('--xtensa-cc', type=Path, action='append',
        help='Repeat to qualify multiple compilers; otherwise use NATIVE_APP_CC/discovery')
    parser.add_argument('--output-dir', type=Path, default=ROOT/'build/usb-native-target')
    args = parser.parse_args()
    sdk = args.msc_sdk.resolve()
    usb.validate_sdk(sdk/usb.MSC_HEADER, True)
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    compilers = [compiler_path(path) for path in args.xtensa_cc] if args.xtensa_cc else [compiler_path()]
    frozen = {name: digest(ROOT/name) for name in usb.NATIVE_SOURCES}
    rows, commands = [], []
    baseline_manifest = subprocess.check_output(['git', 'show', BASELINE+':Apps/native/usb_sd_transfer.json'], cwd=ROOT)
    require((ROOT/'Apps/native/usb_sd_transfer.json').read_bytes() == baseline_manifest,
            'The baseline manifest was changed')

    def run(command, log, **kwargs):
        commands.append(list(map(str, command)))
        result = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, **kwargs)
        log.write_text(result.stdout)
        if result.returncode:
            raise RuntimeError(f'Command failed ({result.returncode}); see {log}:\n{result.stdout[-6000:]}')
        return result.stdout

    with tempfile.TemporaryDirectory(prefix='usb-native-target-') as temporary:
        tmp = Path(temporary)
        baseline = tmp/'baseline'
        subprocess.run(['git', 'clone', '--shared', '--no-checkout', '--quiet', str(ROOT), str(baseline)], check=True)
        subprocess.run(['git', '-C', str(baseline), 'checkout', '--detach', '--quiet', BASELINE], check=True)
        for index, cc in enumerate(compilers):
            compiler = subprocess.check_output([cc, '--version'], text=True).splitlines()[0]
            compiler_id = 'compiler-'+str(index+1)
            folder = out/compiler_id
            folder.mkdir(exist_ok=True)
            environment = dict(os.environ, NATIVE_APP_CC=cc)
            baseline_hashes = {}
            for touch in (False, True):
                label = 'baseline-'+('touch' if touch else 'navigation')
                build_dir = tmp/compiler_id/label
                command = [sys.executable, str(baseline/'scripts/build_portable_usb_transfer.py'),
                    '--msc-sdk', str(sdk), '--output-dir', str(build_dir), *(['--touch-only'] if touch else [])]
                run(command, folder/(label+'.log'), env=environment, cwd=baseline)
                baseline_hashes[touch] = digest(build_dir/'usb_sd_transfer.elf')
                shutil.copyfile(build_dir/'usb_sd_transfer-build-record.json', folder/(label+'-build-record.json'))
            for native in (False, True):
                for snapshot in (False, True):
                    for touch in (False, True):
                        label = ('native' if native else 'off')+'-'+('snapshot' if snapshot else 'direct')+'-'+('touch' if touch else 'navigation')
                        receipts = []
                        for repeat in (1, 2):
                            build_dir = tmp/compiler_id/(label+'-'+str(repeat))
                            command = [sys.executable, str(ROOT/'scripts/build_portable_usb_transfer.py'),
                                '--msc-sdk', str(sdk), '--output-dir', str(build_dir),
                                *(['--native-ui'] if native else []),
                                *(['--raster-snapshot'] if snapshot else []),
                                *(['--touch-only'] if touch else [])]
                            run(command, folder/(label+'-'+str(repeat)+'.log'), env=environment, cwd=ROOT)
                            record = json.loads((build_dir/'usb_sd_transfer-build-record.json').read_text())
                            require(record['version'] == '0.1.11', label+' version changed')
                            require(record['sha256'] == digest(build_dir/'usb_sd_transfer.elf'), label+' ELF receipt mismatch')
                            for name, expected in record['artifact_sha256'].items():
                                require(digest(build_dir/name) == expected, label+' artifact hash mismatch: '+name)
                            require(record['loader_validation']['passed'], label+' loader not validated')
                            require(record['loader_validation']['negative_controls'], label+' loader negative controls missing')
                            require(set(record['exports']) == {'app_main', 'app_module_init', 'app_module_fini'}, label+' exports changed')
                            require(set(record['imports']) <= usb.ALLOWED_IMPORTS, label+' unsupported import')
                            require(record['private_query']['present'] == native and not record['private_query']['dynamic_export'], label+' private query mismatch')
                            require(('native_ui' in record) == native, label+' native receipt leaked/missing')
                            require(('raster_snapshot' in record) == snapshot, label+' raster receipt mismatch')
                            require(('-DPORTABLE_RASTER_SNAPSHOT' in record['defines']) == snapshot, label+' snapshot selector ignored')
                            require(all((flag in record['defines']) == native for flag in usb.NATIVE_FLAGS), label+' native selector mismatch')
                            manifest = json.loads((build_dir/'usb_sd_transfer.json').read_text())
                            expected_manifest = json.loads(baseline_manifest)
                            if touch:
                                expected_manifest['requires'] = [r for r in expected_manifest['requires'] if r['capability'] != 'input.navigation']
                            require(manifest == expected_manifest, label+' manifest or grants changed')
                            require(usb.MSC_HEADER in record['compiled_sdk_sha256'], label+' MSC compile dependency not bound')
                            require('lib/NativeApps/include/T5AppApi.h' in record['compiled_source_sha256'], label+' native API compile dependency not bound')
                            if native:
                                require(record['native_ui']['source_sha256'] == frozen, label+' source changed since profile freeze')
                                require(set(usb.NATIVE_SOURCES) <= set(record['source_sha256']), label+' source dependencies missing')
                                for key in ('hardware_verified', 'product_bound', 'released_artifact', 'version_claim', 'grants_added'):
                                    require(record['native_ui'][key] is False, label+' unsupported claim: '+key)
                                for name in ('Apps/UsbTransferNative.h', 'Apps/UsbTransferLayout.h', 'Apps/usb_sd_transfer_native.inc',
                                             'lib/PortableApps/src/nova_native.inc', 'lib/PortableApps/settings_fonts/text-faces-012.inc'):
                                    require(name in record['compiled_source_sha256'], label+' required compiled source missing: '+name)
                            else:
                                require('Apps/UsbTransferNative.h' not in record['compiled_source_sha256'], label+' private native header leaked into opt-out')
                                require('Apps/usb_sd_transfer_native.inc' not in record['compiled_source_sha256'], label+' native implementation leaked into opt-out')
                            receipts.append(record)
                            shutil.copyfile(build_dir/'usb_sd_transfer-build-record.json', folder/(label+'-'+str(repeat)+'-build-record.json'))
                            if repeat == 1:
                                shutil.copyfile(build_dir/'usb_sd_transfer.json', folder/(label+'-manifest.json'))
                                nm = cc.removesuffix('gcc')+'nm'
                                run([nm, '-D', str(build_dir/'usb_sd_transfer.elf')], folder/(label+'-dynamic-symbols.log'))
                                run([cc.removesuffix('gcc')+'readelf', '-h', '-s', '-W', str(build_dir/'usb_sd_transfer.elf')], folder/(label+'-readelf.log'))
                        require(receipts[0]['artifact_sha256'] == receipts[1]['artifact_sha256'], label+' nondeterministic repeat')
                        if not native and not snapshot:
                            require(receipts[0]['sha256'] == baseline_hashes[touch], label+' pristine feature-off ELF changed')
                        rows.append({'compiler_id': compiler_id, 'compiler': compiler, 'profile': label,
                            'native_ui': native, 'snapshot': snapshot, 'touch_only': touch,
                            'repeats': 2, 'deterministic_artifacts': True, 'strict_warnings': True,
                            'loader_validation': True, 'private_query_correct': True, 'exact_exports': True,
                            'manifest_and_grants_preserved': True,
                            'pristine_baseline_elf_identical': True if not native and not snapshot else None,
                            'sha256': receipts[0]['sha256'], 'size_bytes': receipts[0]['size_bytes']})
                        print(compiler_id+' '+label+': strict ABI/loader, source binding and deterministic repeat PASS', flush=True)
    require(frozen == {name: digest(ROOT/name) for name in usb.NATIVE_SOURCES}, 'Source profile changed during target matrix')
    result = {'schema': 1, 'source_only': True, 'product_bound': False, 'hardware_verified': False,
        'version_claim': False, 'baseline_version': '0.1.11', 'baseline_commit': BASELINE,
        'canonical_sdk_sha256': usb.MSC_SHA256, 'frozen_source_sha256': frozen,
        'commands': commands, 'results': rows,
        'strict_builds': len(rows)*2, 'pristine_baseline_builds': len(compilers)*2}
    (out/'results.json').write_text(json.dumps(result, indent=2)+'\n')
    print(f'PASS: {len(rows)*2} strict target builds; {len(rows)} deterministic pairs; {len(compilers)*2} pristine direct feature-off comparisons')


if __name__ == '__main__':
    main()

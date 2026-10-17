#!/usr/bin/env python3
"""Publish one immutable standalone development bundle from qualified CI output.

Called only by the main-push job after qualification. Existing releases are
verified and skipped; a later source SHA is never relabelled as that release.
Authentication is supplied to the official gh CLI by the workflow environment.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import zipfile

from build_native_wifi_qualification import (ALLOWED_IMPORTS, EXPORTS, PINS, REPOSITORIES,
    manifest as expected_manifest, validate_recipe)

TAG = 'native-wifi-1.4.1-dev1'
DIRECT = ('wifi_settings.elf', 'wifi_settings.json', 'command.json',
          'wifi-build-record.json', 'elf-layout.json', 'elf-memory-admission.json')
REQUIRED = {*DIRECT, 'publication.json', 'SHA256SUMS', TAG+'.zip'}


def gh(*args, check=True):
    return subprocess.run(['gh', *args], check=check, capture_output=True, text=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--artifact', type=Path, required=True)
    parser.add_argument('--repository', required=True)
    parser.add_argument('--source-sha', required=True)
    args = parser.parse_args()
    prior = gh('release', 'view', TAG, '--repo', args.repository,
               '--json', 'tagName,isDraft,isPrerelease,assets', check=False)
    if prior.returncode == 0:
        release = json.loads(prior.stdout)
        if release['isDraft'] or not release['isPrerelease']:
            raise RuntimeError('Existing tag is not a completed development prerelease')
        if not REQUIRED <= {asset['name'] for asset in release['assets']}:
            raise RuntimeError('Existing immutable release is incomplete; manual review required')
        with tempfile.TemporaryDirectory() as temporary:
            gh('release', 'download', TAG, '--repo', args.repository,
               '--pattern', 'publication.json', '--dir', temporary)
            published = json.loads((Path(temporary)/'publication.json').read_text())
        tagged = gh('api', 'repos/'+args.repository+'/commits/'+TAG, '--jq', '.sha').stdout.strip()
        if published['tag'] != TAG or published['source_commit'] != tagged:
            raise RuntimeError('Existing release metadata does not match its immutable tag')
        print('Verified existing '+TAG+' at '+tagged+'; no assets changed. '
              'Current qualification remains associated with '+args.source_sha+'.')
        return
    if 'not found' not in prior.stderr.lower() and '404' not in prior.stderr:
        raise RuntimeError('Release lookup failed: '+prior.stderr)
    tag = gh('api', 'repos/'+args.repository+'/git/ref/tags/'+TAG, check=False)
    if tag.returncode == 0:
        tagged = gh('api', 'repos/'+args.repository+'/commits/'+TAG, '--jq', '.sha').stdout.strip()
        if tagged != args.source_sha:
            raise RuntimeError('Existing tag points to another source; no release was created')
    if tag.returncode and '404' not in tag.stderr:
        raise RuntimeError('Tag lookup failed: '+tag.stderr)
    artifact = args.artifact.resolve()
    app = artifact/'app'
    record = json.loads((app/'wifi-build-record.json').read_text())
    manifest = json.loads((app/'wifi_settings.json').read_text())
    if (record['repository_commit'] != args.source_sha or record['working_tree_dirty']
            or record['bundle'] != TAG or record['version'] != '1.4.1'
            or manifest != expected_manifest('1.4.1')):
        raise RuntimeError('Artifact source/version identity differs from qualified main source')
    if (manifest['requires'] != record['required_capabilities']
            or not all(record.get(name) is True for name in (
                'software_target_validation', 'compiled_version_verified', 'native_ui', 'native_quick', 'native_alarm',
                'atomic_wifi_persistence', 'settings_availability', 'wifi_profiles',
                'shared_text_input', 'async_rf_cleanup', 'host_fixture_excluded'))
            or record.get('hardware_verified') is not False
            or record.get('firmware_or_product_replacement') is not False):
        raise RuntimeError('Complete qualified capability contract is missing')
    if record['external_sources'] != {name: {'repository': REPOSITORIES[name], 'commit': pin}
                                      for name, pin in PINS.items()}:
        raise RuntimeError('Qualified dependency pins changed')
    for name in DIRECT:
        if not (app/name).is_file():
            raise RuntimeError('Missing qualified release input: '+name)
    command_bytes = (app/'command.json').read_bytes()
    layout_bytes = (app/'elf-layout.json').read_bytes()
    if (hashlib.sha256(command_bytes).hexdigest() != record['command_sha256']
            or hashlib.sha256(layout_bytes).hexdigest() != record['elf_layout_sha256']):
        raise RuntimeError('Qualified recipe or ELF layout integrity mismatch')
    command = json.loads(command_bytes)
    validate_recipe(command, '1.4.1')
    if [item for item in command if item.startswith('-D')] != record['build_defines']:
        raise RuntimeError('Recorded selectors differ from the qualified command')
    elf = (app/'wifi_settings.elf').read_bytes()
    if (hashlib.sha256(elf).hexdigest() != record['sha256'] or len(elf) != record['size_bytes']
            or elf[:7] != b'\x7fELF\x01\x01\x01' or elf[16:20] != b'\x03\x00\x5e\x00'
            or b'1.4.1\0' not in elf):
        raise RuntimeError('Target ELF integrity or identity mismatch')
    layout = json.loads(layout_bytes)
    if (layout['sha256'] != record['sha256'] or layout['file_size_bytes'] != len(elf)
            or layout['linked_device_memory_fit_qualified'] is not False):
        raise RuntimeError('ELF layout is not tied to the qualified target')
    admission_bytes = (app/'elf-memory-admission.json').read_bytes()
    admission = json.loads(admission_bytes)
    if (admission.get('application') != manifest['id'] or admission.get('version') != manifest['version']
            or admission.get('runtime_commit') != PINS['runtime']
            or admission.get('elf_sha256') != record['sha256']
            or admission.get('elf_layout_sha256') != record['elf_layout_sha256']
            or admission.get('manifest_sha256') != hashlib.sha256((app/'wifi_settings.json').read_bytes()).hexdigest()
            or admission.get('required_capabilities') != manifest['requires']
            or admission.get('images') != {'native': layout}
            or admission.get('linked_device_memory_fit_qualified') is not False):
        raise RuntimeError('Production admission is not tied to the qualified ELF and manifest')
    runs = admission.get('admission_runs', [])
    if len(runs) != 2 or [run.get('sanitized') for run in runs] != [False, True]:
        raise RuntimeError('Normal and sanitized production admission receipts are required')
    for run in runs:
        result = run.get('result', {})
        if (run.get('image') != 'native-wifi'
                or not all(result.get(name) is True for name in (
                    'ordinary_application_admitted', 'missing_import_rejected',
                    'unsafe_state_rejected', 'driver_role_rejected'))
                or result.get('alignment_placements_checked', 0) <= 0
                or result.get('data_allocation_request_bytes') !=
                    layout['section_loader']['data_allocation_request_bytes']
                or result.get('linked_device_memory_fit_qualified') is not False):
            raise RuntimeError('Production admission or allocation placement checks are incomplete')
    if set(record['exports']) != EXPORTS or not set(record['imports']) <= ALLOWED_IMPORTS:
        raise RuntimeError('Qualified target import/export contract changed')
    for relative, expected in record['notice_sha256'].items():
        path = Path(relative)
        if path.is_absolute() or '..' in path.parts or not path.parts or path.parts[0] != 'licenses':
            raise RuntimeError('Invalid license notice path')
        if hashlib.sha256((app/path).read_bytes()).hexdigest() != expected:
            raise RuntimeError('Qualified license notice changed: '+relative)
    if not record['notice_sha256']:
        raise RuntimeError('Qualified license notices are missing')
    with tempfile.TemporaryDirectory() as temporary:
        out = Path(temporary)
        for name in DIRECT:
            shutil.copyfile(app/name, out/name)
        publication = {'schema': 1, 'tag': TAG, 'source_commit': args.source_sha,
                       'qualification': 'software-only', 'hardware_verified': False,
                       'firmware_replacement': False, 'elf_sha256': record['sha256'],
                       'admission_sha256': hashlib.sha256(admission_bytes).hexdigest(),
                       'manifest_sha256': admission['manifest_sha256']}
        (out/'publication.json').write_text(json.dumps(publication, indent=2)+'\n')
        with zipfile.ZipFile(out/(TAG+'.zip'), 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            for path in sorted(artifact.rglob('*')):
                if path.is_file():
                    archive.write(path, path.relative_to(artifact))
            archive.write(out/'publication.json', 'publication.json')
        sums = ''.join(hashlib.sha256(path.read_bytes()).hexdigest()+'  '+path.name+'\n'
                       for path in sorted(out.iterdir()) if path.is_file())
        (out/'SHA256SUMS').write_text(sums)
        notes = ('Standalone Wi-Fi 1.4.1 development/software qualification. '
                 'Exact source: '+args.source_sha+'. Requires all 18 manifest providers, '
                 'deployment-owned bindings and sufficient loader memory. Physical device fit '
                 'and hardware behavior are unqualified. Does not replace product firmware '
                 'or the retained Wave Wi-Fi 1.1.34 package. Wi-Fi requires instance 15 and '
                 'storage namespaces 6 and 1. The archive includes the qualification receipts, '
                 'source recipe and available license notices.')
        gh('release', 'create', TAG, '--repo', args.repository, '--target', args.source_sha,
           '--draft', '--prerelease', '--title', TAG, '--notes', notes)
        gh('release', 'upload', TAG, '--repo', args.repository,
           *[str(out/name) for name in sorted(REQUIRED)])
        gh('release', 'edit', TAG, '--repo', args.repository, '--draft=false')
        print('Published '+TAG+' from exact qualified source '+args.source_sha)


if __name__ == '__main__':
    main()

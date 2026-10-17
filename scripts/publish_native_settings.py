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

TAG = 'native-settings-1.4.0-dev1'
DIRECT = ('settings.elf', 'settings.json', 'command.json',
          'settings-build-record.json', 'memory-admission.json')
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
    record = json.loads((app/'settings-build-record.json').read_text())
    manifest = json.loads((app/'settings.json').read_text())
    if (record['repository_commit'] != args.source_sha or record['working_tree_dirty']
            or record['bundle'] != TAG or manifest['version'] != '1.4.0'):
        raise RuntimeError('Artifact source/version identity differs from qualified main source')
    if (len(manifest['requires']) != 16
            or manifest['requires'] != record['required_capabilities']
            or not record['software_target_validation']):
        raise RuntimeError('Complete qualified capability contract is missing')
    elf = (app/'settings.elf').read_bytes()
    if hashlib.sha256(elf).hexdigest() != record['sha256'] or len(elf) != record['size_bytes']:
        raise RuntimeError('Target ELF integrity mismatch')
    for name in DIRECT:
        if not (app/name).is_file():
            raise RuntimeError('Missing qualified release input: '+name)
    with tempfile.TemporaryDirectory() as temporary:
        out = Path(temporary)
        for name in DIRECT:
            shutil.copyfile(app/name, out/name)
        publication = {'schema': 1, 'tag': TAG, 'source_commit': args.source_sha,
                       'qualification': 'software-only', 'hardware_verified': False,
                       'firmware_replacement': False, 'elf_sha256': record['sha256']}
        (out/'publication.json').write_text(json.dumps(publication, indent=2)+'\n')
        with zipfile.ZipFile(out/(TAG+'.zip'), 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            for path in sorted(artifact.rglob('*')):
                if path.is_file():
                    archive.write(path, path.relative_to(artifact))
            archive.write(out/'publication.json', 'publication.json')
        sums = ''.join(hashlib.sha256(path.read_bytes()).hexdigest()+'  '+path.name+'\n'
                       for path in sorted(out.iterdir()) if path.is_file())
        (out/'SHA256SUMS').write_text(sums)
        notes = ('Standalone Settings 1.4.0 development/software qualification. '
                 'Exact source: '+args.source_sha+'. Requires all 16 manifest providers, '
                 'deployment-owned bindings and sufficient loader memory. Physical device fit '
                 'and hardware behavior are unqualified. Does not replace product firmware '
                 'or the retained Wave 0.1.59 Settings package. The archive includes '
                 'the qualification receipts, source recipe and available license notices.')
        gh('release', 'create', TAG, '--repo', args.repository, '--target', args.source_sha,
           '--draft', '--prerelease', '--title', TAG, '--notes', notes)
        gh('release', 'upload', TAG, '--repo', args.repository,
           *[str(out/name) for name in sorted(REQUIRED)])
        gh('release', 'edit', TAG, '--repo', args.repository, '--draft=false')
        print('Published '+TAG+' from exact qualified source '+args.source_sha)


if __name__ == '__main__':
    main()

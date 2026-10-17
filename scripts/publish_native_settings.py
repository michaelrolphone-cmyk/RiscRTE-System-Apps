#!/usr/bin/env python3
"""Publish one immutable standalone development bundle from qualified CI output.

Called only by the main-push job after qualification. Existing releases are
verified and skipped; a later source SHA is never relabelled as that release.
The separately selected dev1 warning can change only its audited release body.
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

from native_overlay_evidence import validate_native_overlay_evidence

VERSION = '1.4.2'
TAG = 'native-settings-'+VERSION+'-dev1'
DIRECT = ('settings.elf', 'settings.json', 'command.json',
          'settings-build-record.json', 'memory-admission.json')
REQUIRED = {*DIRECT, 'publication.json', 'SHA256SUMS', TAG+'.zip'}

# Downloaded public-byte audit, 2026-10-17. These are the original dev1 assets,
# not a moving source checkout or the separately versioned successor bundle.
DEV1_REPOSITORY = 'michaelrolphone-cmyk/RiscRTE-System-Apps'
DEV1_TAG = 'native-settings-1.4.0-dev1'
DEV1_RELEASE_ID = 415826219
DEV1_SOURCE = 'fd6bc6065f6e409ed2d2ea1858bd56ab6d9e2fb3'
DEV1_ASSETS = {
    'command.json': (3754, '0783a32628acf22d9cb283d6ade6d0f65939cfaaa663f3d2badb2bbf1b4c45b3'),
    'memory-admission.json': (16700, '6102a687522c5e8044d9081efbd4b5a5fc2206bd09f66fcade04e8dbbfca05ff'),
    'native-settings-1.4.0-dev1.zip': (552530, 'fbdcc2512ba010e11a775c2773700d29325aeb53d462b2b1c0fbe4e693671178'),
    'publication.json': (303, '96e943914425686aea4626fb6be08a231f09130f51cb3dbb487bcbd805230043'),
    'settings-build-record.json': (27112, '68528a48dc42993d6f07e0e972b75f05051cf06a75b1e89fa956d086052a541c'),
    'settings.elf': (325660, '0ca2f1315afe2ba797ccfd953de8e85626022e1f94ce89c8d5e19429d35ab13b'),
    'settings.json': (1235, '40a16c9c99cd05e992c440e9e3cf31bd9af8d93f3a00b6df50a3f0e4fd446bb6'),
    'SHA256SUMS': (599, 'e23eef0eee05bdb0af7191303a914c1ce239e42f0064917444029e4e9d89a993'),
}
DEV1_MARKER = '<!-- native-settings-1.4.0-dev1-known-limitation-v1 -->'
DEV1_END_MARKER = '<!-- /native-settings-1.4.0-dev1-known-limitation-v1 -->'
DEV1_WARNING = (DEV1_MARKER+'\n## Known-limited / not recommended\n\n'
    'The preserved native-settings-1.4.0-dev1 has a confirmed queued previsible-touch defect. '
    'A 410x502 host reproducer using the exact production source and a synchronous RGB565 '
    'provider delayed by 600 ms captured a root-page DOWN/UP before the first frame was '
    'visible, then delivered it after completion and unexpectedly opened the date editor. '
    'No persistence was observed: the reproducer recorded zero Settings writes. '
    'This is software evidence; physical hardware behavior remains unqualified.\n\n'
    'Baseline source: '+DEV1_SOURCE+'. '
    'Durable witness: https://tasker-workboard.hazy-auk-4334.chatgpt.site/'
    '?comment=6e12a663-3dca-4f2b-8c8d-9af1c12fb751\n\n'
    'The original tag and all eight assets are preserved. A separately versioned '
    'successor requires its own qualification.\n'+DEV1_END_MARKER+'\n\n')


def gh(*args, check=True, input=None):
    return subprocess.run(['gh', *args], check=check, capture_output=True, text=True, input=input)


def verify_dev1_release(release):
    if (release['id'] != DEV1_RELEASE_ID or release['tag_name'] != DEV1_TAG
            or release['draft'] or not release['prerelease']):
        raise RuntimeError('Audited dev1 release identity changed; no annotation is safe')
    assets = release['assets']
    if (len(assets) != len(DEV1_ASSETS)
            or {asset['name'] for asset in assets} != set(DEV1_ASSETS)):
        raise RuntimeError('Audited dev1 asset set changed')
    for asset in assets:
        size, digest = DEV1_ASSETS[asset['name']]
        if asset['size'] != size or asset.get('digest') != 'sha256:'+digest:
            raise RuntimeError('Audited dev1 asset integrity changed: '+asset['name'])


def annotate_dev1(repository):
    if repository != DEV1_REPOSITORY:
        raise RuntimeError('Dev1 annotation is restricted to its audited repository')
    base = 'repos/'+repository
    endpoint = base+'/releases/'+str(DEV1_RELEASE_ID)

    def read_verified():
        release = json.loads(gh('api', endpoint).stdout)
        verify_dev1_release(release)
        tagged = gh('api', base+'/commits/'+DEV1_TAG, '--jq', '.sha').stdout.strip()
        if tagged != DEV1_SOURCE:
            raise RuntimeError('Audited dev1 source tag changed')
        return release

    before = read_verified()
    original = before.get('body') or ''
    if DEV1_MARKER in original or DEV1_END_MARKER in original:
        if (not original.startswith(DEV1_WARNING) or original.count(DEV1_MARKER) != 1
                or original.count(DEV1_END_MARKER) != 1):
            raise RuntimeError('Dev1 warning marker differs; manual review required')
        print('Verified existing dev1 warning and original audited tag/assets; no changes.')
        return
    expected = DEV1_WARNING+original
    # PATCH is deliberately body-only. Never edit its tag, name or release state,
    # upload assets, delete assets, or create a replacement for this old release.
    gh('api', endpoint, '--method', 'PATCH', '--input', '-',
       input=json.dumps({'body': expected}))
    after = read_verified()
    stable_fields = ('id', 'tag_name', 'target_commitish', 'name', 'draft',
                     'prerelease', 'created_at', 'published_at', 'immutable')
    # Public downloads may change download_count concurrently; compare asset
    # identity/content fields without treating that read-only traffic as edits.
    asset_fields = ('id', 'node_id', 'name', 'label', 'state', 'content_type',
                    'size', 'digest', 'created_at', 'updated_at', 'url', 'browser_download_url')
    def asset_identities(release):
        return {asset['name']: {key: asset.get(key) for key in asset_fields}
                for asset in release['assets']}
    if (after.get('body') != expected
            or any(after.get(key) != before.get(key) for key in stable_fields)
            or asset_identities(after) != asset_identities(before)):
        raise RuntimeError('Dev1 annotation readback changed body/tag/assets; manual review required')
    print('Verified dev1 known-limitation annotation; original body, tag and eight assets preserved.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--artifact', type=Path)
    parser.add_argument('--repository', required=True)
    parser.add_argument('--source-sha')
    parser.add_argument('--annotate-dev1', action='store_true',
                        help='Only annotate the exact audited 1.4.0 dev1 release body')
    args = parser.parse_args()
    if args.annotate_dev1:
        if args.artifact or args.source_sha:
            parser.error('--annotate-dev1 cannot be combined with successor publication inputs')
        annotate_dev1(args.repository)
        return
    if not args.artifact or not args.source_sha:
        parser.error('Successor publication requires --artifact and --source-sha')
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
            or record['bundle'] != TAG or record['version'] != VERSION
            or manifest['version'] != VERSION):
        raise RuntimeError('Artifact source/version identity differs from qualified main source')
    if (len(manifest['requires']) != 16
            or manifest['requires'] != record['required_capabilities']
            or not record['software_target_validation']):
        raise RuntimeError('Complete qualified capability contract is missing')
    command = json.loads((app/'command.json').read_text())
    selected = {'-DPORTABLE_QUICK_NATIVE', '-DPORTABLE_ALARM_NATIVE'}
    if (not selected <= set(command)
            or [item for item in command if item.startswith('-D')] != record.get('build_defines')
            or record.get('native_quick') is not True or record.get('native_alarm') is not True):
        raise RuntimeError('Qualified native overlay selection is missing or inconsistent')
    elf = (app/'settings.elf').read_bytes()
    if hashlib.sha256(elf).hexdigest() != record['sha256'] or len(elf) != record['size_bytes']:
        raise RuntimeError('Target ELF integrity mismatch')
    for name in DIRECT:
        if not (app/name).is_file():
            raise RuntimeError('Missing qualified release input: '+name)
    overlay_evidence = validate_native_overlay_evidence(
        'settings', artifact, record, args.source_sha)
    with tempfile.TemporaryDirectory() as temporary:
        out = Path(temporary)
        for name in DIRECT:
            shutil.copyfile(app/name, out/name)
        publication = {'schema': 1, 'tag': TAG, 'source_commit': args.source_sha,
                       'qualification': 'software-only', 'hardware_verified': False,
                       'firmware_replacement': False, 'elf_sha256': record['sha256'],
                       'native_overlay_evidence': overlay_evidence}
        (out/'publication.json').write_text(json.dumps(publication, indent=2)+'\n')
        with zipfile.ZipFile(out/(TAG+'.zip'), 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            for path in sorted(artifact.rglob('*')):
                if path.is_file():
                    archive.write(path, path.relative_to(artifact))
            archive.write(out/'publication.json', 'publication.json')
        sums = ''.join(hashlib.sha256(path.read_bytes()).hexdigest()+'  '+path.name+'\n'
                       for path in sorted(out.iterdir()) if path.is_file())
        (out/'SHA256SUMS').write_text(sums)
        notes = ('Standalone Settings '+VERSION+' development/software qualification. '
                 'Exact source: '+args.source_sha+'. Requires all 16 manifest providers, '
                 'deployment-owned bindings and sufficient loader memory. Physical device fit '
                 'and hardware behavior are unqualified. Does not replace product firmware '
                 'or the retained Wave 0.1.59 Settings package. The archive includes '
                 'the qualification receipts, source recipe and available license notices. '
                 'Native Quick Controls and alarm overlays are explicitly selected.')
        gh('release', 'create', TAG, '--repo', args.repository, '--target', args.source_sha,
           '--draft', '--prerelease', '--title', TAG, '--notes', notes)
        gh('release', 'upload', TAG, '--repo', args.repository,
           *[str(out/name) for name in sorted(REQUIRED)])
        gh('release', 'edit', TAG, '--repo', args.repository, '--draft=false')
        print('Published '+TAG+' from exact qualified source '+args.source_sha)


if __name__ == '__main__':
    main()

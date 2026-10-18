#!/usr/bin/env python3
"""Prepare, or explicitly publish, one new immutable native App Store prerelease.

No release update, asset replacement, tag move, firmware build or catalog mutation
is supported. Every network response is retained before parsing. Publication uses
a draft, verifies uploaded bytes, then verifies all bytes through public URLs.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
from urllib.request import Request, urlopen
import zipfile

from native_app_store_evidence import TAG, VERSION, DIRECT, ROOT, read, require, sha, validate

REPOSITORY = 'michaelrolphone-cmyk/RiscRTE-System-Apps'
ASSETS = {*DIRECT, 'publication.json', 'catalog-binding.json', 'qualification.json', 'SHA256SUMS', TAG+'.zip'}


def encode(value):
    return json.dumps(value, indent=2, sort_keys=True)+'\n'


class Github:
    def __init__(self, audit):
        self.audit = audit
        self.audit.mkdir(parents=True, exist_ok=True)
        self.index = 0

    def gh(self, *args, check=True):
        self.index += 1
        prefix = self.audit/f'{self.index:03d}'
        result = subprocess.run(['gh', *args], capture_output=True)
        prefix.with_suffix('.command.json').write_text(encode(['gh', *args]))
        prefix.with_suffix('.stdout').write_bytes(result.stdout)
        prefix.with_suffix('.stderr').write_bytes(result.stderr)
        prefix.with_suffix('.status.json').write_text(encode({'returncode': result.returncode}))
        if check:
            result.check_returncode()
        return result

    def releases(self):
        raw = self.gh('api', '--paginate', '--slurp', 'repos/'+REPOSITORY+'/releases?per_page=100')
        pages = json.loads(raw.stdout)
        return [item for page in pages for item in page]

    def commit(self):
        raw = self.gh('api', 'repos/'+REPOSITORY+'/commits/'+TAG)
        return json.loads(raw.stdout)['sha']


def inventory(releases):
    # Preserve every preexisting published/draft release and its asset identities.
    return {r['tag_name']: {k: r.get(k) for k in ('id','tag_name','target_commitish','name','body','draft','prerelease','published_at')}
            | {'assets': sorted([{k: a.get(k) for k in ('id','name','size','digest','browser_download_url')}
                                 for a in r['assets']], key=lambda a: a['name'])}
            for r in releases if r['tag_name'] != TAG}


def binding(record, source_sha):
    base = 'https://github.com/'+REPOSITORY+'/releases/download/'+TAG+'/'
    return {'schema': 1, 'repository': REPOSITORY, 'release_tag': TAG,
            'application': 'app_store', 'version': VERSION, 'source_commit': source_sha,
            'architecture': 'xtensa-esp32s3', 'profile': 'native-app-store',
            'image': {'url': base+'app_store.elf', 'sha256': record['sha256'], 'bytes': record['size_bytes']},
            'manifest_url': base+'app_store.json', 'required_capabilities': record['required_capabilities'],
            'grant_requirements': {'software.update.apps@1': [0], 'net.wifi@1': [15],
                                   'storage.key-value@1': [1,6], 'storage.key-value.atomic@1': [1]},
            'update_semantics': 'additive-entry-only-preserve-all-prior-entries',
            'qualification': 'software-only', 'hardware_verified': False,
            'physical_memory_fit_qualified': False, 'install_path_qualified': False,
            'firmware_payload_change': False, 'catalog_mutation_performed': False}


def prepare(artifact, output, source_sha):
    qualification = validate(artifact, source_sha)
    require(not output.exists() or not any(output.iterdir()), 'use an empty preparation directory')
    output.mkdir(parents=True, exist_ok=True)
    app = artifact/'app'
    record = read(app/'app-store-build-record.json')
    for name in DIRECT:
        shutil.copyfile(app/name, output/name)
    (output/'qualification.json').write_text(encode(qualification))
    catalog = binding(record, source_sha)
    catalog['manifest_sha256'] = sha(app/'app_store.json')
    (output/'catalog-binding.json').write_text(encode(catalog))
    publication = {'schema': 1, 'tag': TAG, 'source_commit': source_sha,
                   'qualification': 'software-only', 'hardware_verified': False,
                   'firmware_replacement': False, 'elf_sha256': record['sha256'],
                   'manifest_sha256': sha(app/'app_store.json'),
                   'qualification_sha256': sha(output/'qualification.json'),
                   'admission_sha256': sha(app/'elf-memory-admission.json'),
                   'catalog_binding_sha256': sha(output/'catalog-binding.json')}
    (output/'publication.json').write_text(encode(publication))
    # Fixed timestamps, order and modes make the archive independent of file mtimes.
    with zipfile.ZipFile(output/(TAG+'.zip'), 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path in sorted(artifact.rglob('*')):
            if path.is_file():
                require(not path.is_symlink(), 'symlink in qualification artifact')
                info = zipfile.ZipInfo('qualification/'+str(path.relative_to(artifact)), (1980,1,1,0,0,0))
                info.compress_type = zipfile.ZIP_DEFLATED; info.external_attr = 0o100644 << 16
                archive.writestr(info, path.read_bytes())
        for name in ('publication.json','catalog-binding.json','qualification.json'):
            info = zipfile.ZipInfo(name, (1980,1,1,0,0,0)); info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, (output/name).read_bytes())
    (output/'SHA256SUMS').write_text(''.join(sha(output/name)+'  '+name+'\n' for name in sorted(ASSETS-{'SHA256SUMS'})))
    verify_bundle(output, source_sha)
    return qualification


def verify_bundle(folder, source_sha):
    require({p.name for p in folder.iterdir() if p.is_file()} == ASSETS, 'release asset set differs')
    sums = {}
    for line in (folder/'SHA256SUMS').read_text().splitlines():
        digest, name = line.split('  ')
        require(name in ASSETS-{'SHA256SUMS'} and name not in sums, 'invalid or duplicate checksum entry')
        require(sha(folder/name) == digest, 'asset checksum mismatch: '+name)
        sums[name] = digest
    require(set(sums) == ASSETS-{'SHA256SUMS'}, 'checksum coverage differs')
    publication = read(folder/'publication.json')
    require(publication['tag'] == TAG and publication['source_commit'] == source_sha,
            'immutable release source differs; choose a new version')
    require(publication['elf_sha256'] == sha(folder/'app_store.elf') and
            publication['manifest_sha256'] == sha(folder/'app_store.json') and
            publication['admission_sha256'] == sha(folder/'elf-memory-admission.json') and
            publication['qualification_sha256'] == sha(folder/'qualification.json') and
            publication['catalog_binding_sha256'] == sha(folder/'catalog-binding.json'), 'publication audit differs')
    with zipfile.ZipFile(folder/(TAG+'.zip')) as archive:
        require(len(archive.namelist()) == len(set(archive.namelist())), 'duplicate archive members')
        for name in DIRECT:
            require(archive.read('qualification/app/'+name) == (folder/name).read_bytes(), 'archive/direct bytes differ: '+name)
        for name in ('publication.json','catalog-binding.json','qualification.json'):
            require(archive.read(name) == (folder/name).read_bytes(), 'archive metadata differs: '+name)
    return {name: sha(folder/name) for name in sorted(ASSETS)}


def public_audit(client, output, source_sha, expected=None):
    target = output/'public-assets'
    target.mkdir(parents=True, exist_ok=False)
    for name in sorted(ASSETS):
        url = 'https://github.com/'+REPOSITORY+'/releases/download/'+TAG+'/'+name
        request = Request(url, headers={'User-Agent':'native-app-store-release-audit'})
        with urlopen(request, timeout=180) as response:
            body = response.read()
            # Retain body and response metadata before parsing even checksum files.
            (target/name).write_bytes(body)
            (output/(name+'.headers.json')).write_text(encode({'status': response.status,
                'request_url': url, 'resolved_url': response.url, 'headers': dict(response.headers.items())}))
            require(response.status == 200, 'public download failed')
    actual = verify_bundle(target, source_sha)
    require(expected is None or actual == expected, 'public bytes differ from uploaded qualification')
    require(client.commit() == source_sha, 'public release tag does not bind the exact source')
    audit = {'schema': 1, 'status': 'PASS', 'tag': TAG, 'source_commit': source_sha,
             'public_asset_sha256': actual, 'all_public_assets_byte_verified': True,
             'hardware_verified': False, 'catalog_updated': False}
    (output/'public-byte-audit.json').write_text(encode(audit))
    return audit


def publish(artifact, prepared, audit, source_sha):
    require(subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'], text=True).strip() == source_sha,
            'publication checkout differs from qualified source')
    require(not subprocess.check_output(['git','-C',str(ROOT),'status','--porcelain']).strip(),
            'publication requires a clean checkout')
    validate(artifact, source_sha)
    expected = verify_bundle(prepared, source_sha)
    client = Github(audit/'network')
    before = client.releases()
    old = inventory(before)
    (audit/'prior-release-inventory.json').write_text(encode(old))
    existing = [r for r in before if r['tag_name'] == TAG]
    if existing:
        require(len(existing) == 1 and not existing[0]['draft'] and existing[0]['prerelease'],
                'existing release incomplete; leave it untouched for manual review')
        require(client.commit() == source_sha, 'existing tag has another source; choose a new version')
        # Exact-source reruns must match the newly verified bundle byte-for-byte.
        # Reproducibility drift is a blocker, never authority to replace assets.
        result = public_audit(client, audit, source_sha, expected)
        print('Verified existing immutable release; no release assets changed')
    else:
        ref = client.gh('api', 'repos/'+REPOSITORY+'/git/ref/tags/'+TAG, check=False)
        require(ref.returncode != 0 and b'404' in ref.stderr, 'tag already exists or cannot be checked; do not reuse it')
        notes = ('Standalone native App Store '+VERSION+' software qualification. Exact source: '+source_sha+'. '
                 'All 16 capability providers and deployment-owned grants are required. '
                 'Physical memory fit, device behavior, install path and firmware composition are unqualified. '
                 'This additive development release does not replace or modify earlier releases.')
        client.gh('api','--method','POST','repos/'+REPOSITORY+'/git/refs',
                  '-f','ref=refs/tags/'+TAG,'-f','sha='+source_sha)
        client.gh('release','create',TAG,'--repo',REPOSITORY,'--target',source_sha,
                  '--draft','--prerelease','--title',TAG,'--notes',notes)
        client.gh('release','upload',TAG,'--repo',REPOSITORY,*[str(prepared/name) for name in sorted(ASSETS)])
        require(client.commit() == source_sha, 'draft tag differs from qualified source')
        # Verify every draft byte before making it public. Never clobber or delete.
        with tempfile.TemporaryDirectory() as tmp:
            client.gh('release','download',TAG,'--repo',REPOSITORY,'--dir',tmp)
            require(verify_bundle(Path(tmp),source_sha) == expected, 'draft upload byte mismatch')
        client.gh('release','edit',TAG,'--repo',REPOSITORY,'--draft=false')
        result = public_audit(client,audit,source_sha,expected)
    after = inventory(client.releases())
    require(after == old, 'a prior release changed during publication; investigate before catalog binding')
    (audit/'prior-release-preservation.json').write_text(encode({'status':'PASS', 'before':old, 'after':after}))
    print(encode(result))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--artifact', type=Path, required=True)
    parser.add_argument('--source-sha', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--publish', action='store_true', help='Explicitly publish after all offline checks')
    parser.add_argument('--audit', type=Path, help='New directory for retained network responses and public audit')
    args = parser.parse_args()
    artifact, output = args.artifact.resolve(), args.output.resolve()
    require(not output.is_relative_to(artifact), 'preparation output cannot be inside its input artifact')
    qualification = prepare(artifact,output,args.source_sha)
    print(encode(qualification))
    if args.publish:
        require(args.audit is not None, 'publication requires an audit directory')
        audit = args.audit.resolve()
        require(not audit.exists(), 'use a new audit directory')
        audit.mkdir(parents=True)
        publish(artifact,output,audit,args.source_sha)
    else:
        print('Prepared and verified local bundle; no release or catalog was changed')


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Optional host-only ZIP witness; no network, publishing or runtime imports."""
import argparse
import copy
import hashlib
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
IDENTITY = 'button_remap'


def digest(data):
    return hashlib.sha256(data).hexdigest()


def blob(data):
    return hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reader', type=Path, required=True, help='Read-only Git checkout containing pinned U1 tools')
    args = parser.parse_args()
    lock = json.loads((ROOT / 'sdk/prospective-u1-tools.json').read_text())
    baseline = json.loads((ROOT / 'sdk/release-baseline.json').read_text())
    published = next(row for row in baseline['apps'] if row['id'] == IDENTITY)
    evidence = json.loads((ROOT / 'dist/apps/build-evidence.json').read_text())
    built = next(row for row in evidence['apps'] if row['id'] == IDENTITY)
    payloads = {name: (ROOT / 'dist/apps' / name).read_bytes()
                for name in (IDENTITY + '.elf', IDENTITY + '.json')}
    elf, sidecar = payloads[IDENTITY + '.elf'], json.loads(payloads[IDENTITY + '.json'])
    require(digest(elf) == published['sha256'] == built['sha256'] == sidecar['sha256'], 'ELF identity differs from pinned published/build/sidecar identity')
    require(len(elf) == published['size'] == built['size_bytes'] == sidecar['size_bytes'], 'ELF size disagreement')
    require(sidecar['version'] == built['version'] == published['version'], 'version disagreement')
    for ext, key in [('c', 'source_blob'), ('json', 'manifest_blob')]:
        require(blob((ROOT / 'Apps' / (IDENTITY + '.' + ext)).read_bytes()) == built[key], 'stale source/build provenance')
    source_manifest = json.loads((ROOT / 'Apps' / (IDENTITY + '.json')).read_text())
    require({k: v for k, v in sidecar.items() if k not in ('size_bytes', 'sha256')} == source_manifest, 'sidecar changed source metadata')
    with tempfile.TemporaryDirectory(prefix='prospective-u1-witness-') as temp:
        workspace = Path(temp)
        tools = workspace / 'tools'
        tools.mkdir()
        for row in lock['files']:
            data = subprocess.check_output(['git', '-C', str(args.reader), 'show', lock['commit'] + ':' + row['path']], timeout=60)
            require(blob(data) == row['git_blob'] and digest(data) == row['sha256'], 'pinned tool content mismatch: ' + row['path'])
            (tools / Path(row['path']).name).write_bytes(data)
        sys.path.insert(0, str(tools))
        from pack_rte_zip import pack_directory
        from update_release_index import validate_bundle_manifest
        from app_manifest import package_requirements, validate_manifest
        validate_manifest(ROOT / 'Apps' / (IDENTITY + '.c'), Path(IDENTITY + '.elf'))
        manifest = dict(schema=1, kind='application', id=IDENTITY, version=sidecar['version'],
                        artifact=IDENTITY + '.elf', architecture='xtensa-esp32s3', min_runtime_api=2,
                        entries=[dict(name=name, size_bytes=len(data), sha256=digest(data), executable=name.endswith('.elf'))
                                 for name, data in payloads.items()], requires=package_requirements(sidecar))
        validate_bundle_manifest(manifest, IDENTITY, sidecar['version'], 'xtensa-esp32s3', 'application')
        package = workspace / 'package'
        package.mkdir()
        payloads['.package.json'] = (json.dumps(manifest, separators=(',', ':'), ensure_ascii=True) + '\n').encode('ascii')
        for name, data in payloads.items():
            (package / name).write_bytes(data)
        archive = pack_directory(package)
        require(archive == pack_directory(package), 'archive is not deterministic')
        with zipfile.ZipFile(io.BytesIO(archive)) as zipped:
            require(zipped.namelist() == ['.package.json', IDENTITY + '.elf', IDENTITY + '.json'], 'ZIP inventory mismatch')
            require(zipped.testzip() is None, 'ZIP CRC failure')
            for name, data in payloads.items():
                require(zipped.read(name) == data, 'ZIP payload changed: ' + name)
            require(all(row.compress_type == zipfile.ZIP_STORED for row in zipped.infolist()), 'unexpected compression')
        # Meaningful fail-closed checks using the actual pinned producer/validator.
        (package / (IDENTITY + '.elf')).write_bytes(elf[:-1] + bytes([elf[-1] ^ 1]))
        try:
            pack_directory(package)
        except ValueError:
            pass
        else:
            raise ValueError('corrupt payload accepted')
        malformed = copy.deepcopy(manifest)
        malformed['entries'][0]['name'] = '../escape.elf'
        try:
            validate_bundle_manifest(malformed, IDENTITY, sidecar['version'], 'xtensa-esp32s3', 'application')
        except ValueError:
            pass
        else:
            raise ValueError('escaping manifest accepted')
    output = ROOT / 'build/prospective-u1'
    output.mkdir(parents=True, exist_ok=True)
    name = IDENTITY + '.rte.zip'
    (output / name).write_bytes(archive)
    report = dict(purpose='prospective-host-only-unpublished-witness', tools=lock,
                  external_commit=subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip(),
                  build_repository_commit=evidence['repository_commit'], build_working_tree_dirty=evidence['working_tree_dirty'],
                  sdk_commit=evidence['sdk_commit'], compiler=evidence['compiler'], source=built,
                  release_index_commit=baseline['release_index_commit'], manifest=manifest,
                  archive=dict(name=name, size_bytes=len(archive), sha256=digest(archive)),
                  checks=['pinned-tool-blobs', 'released-ELF-identity', 'source-sidecar-identity', 'upstream-manifest-validator',
                          'deterministic-upstream-packer', 'stored-ZIP-CRC-and-exact-payloads', 'corrupt-payload-rejected', 'path-traversal-rejected'],
                  limits=['No device/runtime installer or dependency activation tested', 'No SDK compatibility certification',
                          'No publication, catalog or ownership switch', 'No production package identity reassigned'])
    (output / 'witness.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f"Prospective U1 ZIP witness PASS: {IDENTITY} {sidecar['version']}; {len(archive)} bytes; {digest(archive)}")


if __name__ == '__main__':
    main()

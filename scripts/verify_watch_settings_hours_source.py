#!/usr/bin/env python3
"""Verify the exact hour-format source and its public code mapping."""
import argparse
import hashlib
import json
import tempfile
from pathlib import Path
import verify_watch_alarm_source as alarm

ROOT = Path(__file__).resolve().parents[1]
CUSTODY = ROOT / 'docs/source-checkpoints/watch-settings-hours'
SOURCE = 'df1d7d1b248de13f773690139750ad2f963967f3'
TREE = '9250000672f989be18f2ef45d48c257980c18604'
PUBLIC = 'a290ca19c56e472989a54db0ddfe9580c9ff792c'
PUBLIC_TREE = '106ffe4fe18b4c52027139c285378c2663fd04aa'
PUBLIC_PARENT = '6c76d72d2c670461d9cbfc3bd5a9e9664b4d41e8'
DIGEST = 'aeb380c81218b0b43fbb8c4bd12d870803f70ef7c6e0bae401d565a6b6a4a512'
SIZE = 2535
PRIOR_CUSTODY_FILES = alarm.BASE_CUSTODY_FILES | {
    '.github/workflows/watch-alarm-retention.yml',
    'docs/source-checkpoints/watch-alarm-terminal/README.md',
    'docs/source-checkpoints/watch-alarm-terminal/System.bundle',
    'docs/source-checkpoints/watch-alarm-terminal/manifest.json',
    'scripts/verify_watch_alarm_source.py', 'tests/test_watch_alarm_source.py',
}


def read_custody(directory=CUSTODY):
    manifest = json.loads((directory / 'manifest.json').read_text())
    expected = {
        'schema': 1, 'repository': 'michaelrolphone-cmyk/RiscRTE-System-Apps',
        'bundle': 'System.bundle', 'bundle_bytes': SIZE, 'bundle_sha256': DIGEST,
        'prerequisite_commit': alarm.SOURCE, 'bundle_ref': 'HEAD',
        'source_commit': SOURCE, 'source_tree': TREE, 'public_code_commit': PUBLIC,
        'public_code_tree': PUBLIC_TREE, 'public_code_parent': PUBLIC_PARENT,
    }
    alarm.require(manifest == expected, 'Custody manifest differs from fixed source mapping')
    bundle = directory / 'System.bundle'
    payload = bundle.read_bytes()
    alarm.require(len(payload) == SIZE, 'Bundle byte count mismatch')
    alarm.require(hashlib.sha256(payload).hexdigest() == DIGEST, 'Bundle SHA-256 mismatch')
    header, separator, _ = payload.partition(b'\n\n')
    expected_header = (f'# v2 git bundle\n-{alarm.SOURCE} Fence selected Watch alarm clients '
                       f'on terminal retention\n{SOURCE} HEAD').encode()
    alarm.require(separator and header == expected_header, 'Bundle prerequisite/ref mismatch')
    return manifest, bundle


def verify(repository=ROOT):
    manifest, bundle = read_custody()
    _, alarm_bundle = alarm.read_custody()
    _, base_bundle = alarm.read_base()
    repository = repository.resolve()
    git, require = alarm.git, alarm.require
    require(git(repository, 'rev-parse', f'{PUBLIC}^{{tree}}').stdout.strip() == PUBLIC_TREE,
            'Public tree mismatch')
    require(git(repository, 'rev-parse', f'{PUBLIC}^').stdout.strip() == PUBLIC_PARENT,
            'Public parent mismatch')
    with tempfile.TemporaryDirectory(prefix='watch-settings-hours-source-') as temporary:
        fresh = Path(temporary)
        git(fresh, 'init', '--bare', '--quiet')
        git(fresh, 'fetch', '--quiet', '--no-tags', str(repository),
            f'{alarm.PUBLIC_PREREQUISITE}:refs/heads/prerequisite')
        require(git(fresh, 'cat-file', '-e', f'{SOURCE}^{{commit}}', check=False).returncode != 0,
                'Source unexpectedly present before import')
        for name, item in [('base', base_bundle), ('alarm', alarm_bundle), ('hours', bundle)]:
            git(fresh, 'bundle', 'verify', str(item))
            git(fresh, 'fetch', '--quiet', '--no-tags', str(item), f'HEAD:refs/heads/{name}')
        require(git(fresh, 'rev-parse', 'hours').stdout.strip() == SOURCE, 'Imported source mismatch')
        require(git(fresh, 'rev-parse', f'{SOURCE}^{{tree}}').stdout.strip() == TREE,
                'Imported tree mismatch')
        require(git(fresh, 'rev-parse', f'{SOURCE}^').stdout.strip() == alarm.SOURCE,
                'Imported source parent mismatch')
        git(fresh, 'fetch', '--quiet', '--no-tags', str(repository), f'{PUBLIC}:refs/heads/public-code')
        changes = git(fresh, 'diff', '--name-status', SOURCE, PUBLIC).stdout.splitlines()
        require(set(changes) == {'A\t' + path for path in PRIOR_CUSTODY_FILES},
                'Public code differs beyond existing source custody files')
        git(fresh, 'fsck', '--full', '--strict')
    return dict(manifest, verified=True, fresh_repository_import=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repository', type=Path, default=ROOT)
    print(json.dumps(verify(parser.parse_args().repository), indent=2))

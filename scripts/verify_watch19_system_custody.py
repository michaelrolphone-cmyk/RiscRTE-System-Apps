#!/usr/bin/env python3
"""Verify the fixed Watch19 source bundle without changing this checkout."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
CUSTODY = ROOT / 'docs/source-checkpoints/watch19-system'
PREREQUISITE = '851f389d72672f1ff33825940306b1a4e9931062'
SOURCE = '1b1c2009935a5489c897b12c12bd4a37e684779f'
TREE = '8a3337a96c6b78b1e39421424a61ea4bdda82091'
PUBLIC = 'dbbd479eb329bb54a0cb21d1e4d16bd930e4dc4d'
PARENT = 'c12ee9cfac111cde2160132faa110c67e57f2fc7'
DIGEST = '5d59a53b22e5f6f92f46d6c58e3d3138ee910d8e8cd9d36b22bbc7d456075f4f'
SIZE = 25741


def require(condition, message):
    if not condition:
        raise ValueError(message)


def read_custody(directory=CUSTODY):
    """Reject changes to the original bytes or their fixed identity mapping."""
    manifest = json.loads((directory / 'manifest.json').read_text())
    expected = {
        'schema': 1,
        'repository': 'michaelrolphone-cmyk/RiscRTE-System-Apps',
        'bundle': 'System.bundle',
        'bundle_bytes': SIZE,
        'bundle_sha256': DIGEST,
        'prerequisite_commit': PREREQUISITE,
        'bundle_ref': 'HEAD',
        'reconstruction_commit': SOURCE,
        'source_tree': TREE,
        'public_code_commit': PUBLIC,
        'public_code_parent': PARENT,
        'original_commit': '3c9e6dbe54d4d744ef31511e7994890c900b808b',
        'original_commit_recovered': False,
    }
    require(manifest == expected, 'Custody manifest differs from the fixed source mapping')
    bundle = directory / 'System.bundle'
    payload = bundle.read_bytes()
    require(len(payload) == SIZE, 'Bundle byte count mismatch')
    require(hashlib.sha256(payload).hexdigest() == DIGEST, 'Bundle SHA-256 mismatch')
    header, separator, _ = payload.partition(b'\n\n')
    expected_header = (
        '# v2 git bundle\n'
        f'-{PREREQUISITE} Keep standalone Nova render fixtures independent of adapter capture hooks\n'
        f'{SOURCE} HEAD'
    ).encode()
    require(separator and header == expected_header,
            'Bundle must contain exactly the recorded prerequisite and source ref')
    return manifest, bundle


def git(repository, *args, check=True):
    result = subprocess.run(['git', '-C', str(repository), *args],
                            text=True, capture_output=True)
    if check and result.returncode:
        raise RuntimeError(f"Git {' '.join(args)} failed: {result.stderr.strip()}")
    return result


def verify(repository=ROOT):
    manifest, bundle = read_custody()
    repository = repository.resolve()
    require(git(repository, 'rev-parse', f'{PUBLIC}^{{tree}}').stdout.strip() == TREE,
            'Public source commit tree mismatch')
    require(git(repository, 'rev-parse', f'{PUBLIC}^').stdout.strip() == PARENT,
            'Public source commit parent mismatch')
    with tempfile.TemporaryDirectory(prefix='watch19-system-custody-') as directory:
        fresh = Path(directory)
        git(fresh, 'init', '--bare', '--quiet')
        # Fetch only the public prerequisite and its ancestors. No alternates,
        # recovery refs, or local reconstructed objects are supplied.
        git(fresh, 'fetch', '--quiet', '--no-tags', str(repository),
            f'{PREREQUISITE}:refs/heads/prerequisite')
        require(git(fresh, 'cat-file', '-e', f'{SOURCE}^{{commit}}',
                    check=False).returncode != 0,
                'Fresh prerequisite repository unexpectedly contains the source commit')
        git(fresh, 'bundle', 'verify', str(bundle))
        git(fresh, 'fetch', '--quiet', '--no-tags', str(bundle),
            'HEAD:refs/heads/watch19-system-reconstruction')
        require(git(fresh, 'rev-parse', 'watch19-system-reconstruction').stdout.strip() == SOURCE,
                'Imported source commit mismatch')
        require(git(fresh, 'rev-parse', f'{SOURCE}^{{tree}}').stdout.strip() == TREE,
                'Imported source tree mismatch')
        git(fresh, 'merge-base', '--is-ancestor', PREREQUISITE, SOURCE)
        git(fresh, 'fsck', '--full', '--strict')
    return dict(manifest, verified=True, source_absent_before_import=True,
                fresh_repository_import=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repository', type=Path, default=ROOT,
                        help='Full public repository containing the prerequisite and public code commit')
    args = parser.parse_args()
    print(json.dumps(verify(args.repository), indent=2))

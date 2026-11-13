"""Verify a clean descendant checkout against immutable canonical SDK inputs, allowing additive files.

This is SDK compatibility only, never identity of native implementations.
Consumers must stage immutable canonical blobs rather than checkout files.
"""
import subprocess


def verify_runtime_sdk(repo, canonical):
    def git(*args):
        return subprocess.check_output(['git', '-C', str(repo), *args],
                                       text=True, stderr=subprocess.PIPE).strip()
    head = git('rev-parse', 'HEAD')
    canonical = git('rev-parse', canonical + '^{commit}')
    if git('status', '--porcelain'):
        raise ValueError('Runtime SDK requires a clean checkout')
    if subprocess.run(['git', '-C', str(repo), 'merge-base', '--is-ancestor',
                       canonical, head], stdout=subprocess.PIPE, stderr=subprocess.PIPE).returncode:
        raise ValueError('Runtime checkout is not a descendant of the canonical SDK revision')
    trees = {}
    additions = {}
    def entries(commit):
        # Compare modes and blob identities as well as paths. Existing files
        # cannot be removed, replaced with symlinks, or silently changed.
        rows = git('ls-tree', '-r', '-z', commit, '--', 'sdk')
        return {row.split('\t',1)[1]:row.split('\t',1)[0]
                for row in rows.split('\0') if row}
    baseline = entries(canonical)
    current = entries(head)
    for name, identity in baseline.items():
        if current.get(name) != identity:
            raise ValueError('Runtime canonical SDK mismatch: ' + name)
    additions = {name:identity for name,identity in current.items() if name not in baseline}

    for path in ('sdk', 'LICENSE'):
        expected = git('rev-parse', canonical + ':' + path)
        actual = git('rev-parse', head + ':' + path)
        if actual != expected and path != 'sdk':
            raise ValueError('Runtime canonical SDK mismatch: ' + path)
        trees[path] = expected
    return {'canonical_commit': canonical, 'checkout_commit': head,
            'checkout_clean': True, 'canonical_ancestor': True,
            **({'canonical_git_objects': trees, 'unchanged_canonical_sdk_files': len(baseline),
                'additive_sdk_files': additions, 'checkout_sdk_object': git('rev-parse', head + ':sdk')}
               if additions else {'exact_git_objects': trees}),
            'scope': 'All canonical SDK file modes/blob identities and LICENSE unchanged; additions are not staged implicitly; native implementation may differ'}

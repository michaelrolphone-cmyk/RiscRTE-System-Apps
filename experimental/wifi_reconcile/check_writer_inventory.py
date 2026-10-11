#!/usr/bin/env python3
"""Read-only check of the exact selected manifest/image/source audit inputs.

This verifies evidence identity and audit coverage, not semantic correctness of
manual classifications, target execution or readiness to enable persistence.
"""
import argparse
import copy
import hashlib
import json
import subprocess
from pathlib import Path


def require(ok, message):
    if not ok:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read(path):
    return json.loads(path.read_text())


def image_path(row, manifest, provider=False):
    if 'image_source' in row:
        return 'SD/' + row['image_source']['path']
    name = str(Path(row['manifest']).parent / 'driver.elf') if provider else manifest['file_name']
    return 'store/' + name


def validate(record, roots):
    c = roots['composition']
    boot = read(c / 'store/boot.json')
    require(record['boot_sha256'] == digest(c / 'store/boot.json'), 'boot changed')
    require(not boot['installation'].get('shared_settings'), 'shared settings became enabled')
    composition = read(c / 'receipts/shared-settings-composition.json')
    require(composition['shared_settings_enabled'] is False, 'composition enables shared settings')
    require(record['shared_settings_enabled'] is False, 'inventory enables shared settings')
    require(record['product_version'] == composition['product_version'], 'product changed')
    for name, revision in record['composition_source_commits'].items():
        require(composition[name + '_commit'] == revision, name + ' composition source changed')
    seed = read(roots['seed'] / 'qualification.json')
    sd_seed = {v['path']: v['sha256'] for v in read(roots['seed'] / 'receipts/sd-matching.json')}
    for field, selected_field, provider in [('apps', 'app_capabilities', False), ('providers', 'drivers', True)]:
        actual = {r['manifest']: r for r in boot[selected_field]}
        listed = {r['manifest']: r for r in record[field]}
        require(len(listed) == len(record[field]), field + ' duplicate inventory entry')
        require(actual.keys() == listed.keys(), field + ' missing/extra selected entries')
        for name, item in listed.items():
            selected = actual[name]
            manifest_path = c / 'store' / name
            manifest = read(manifest_path)
            require(digest(manifest_path) == item['manifest_sha256'], name + ' manifest changed')
            require(manifest.get('version') == item['version'], name + ' version changed')
            image = image_path(selected, manifest, provider)
            require(image == item['selected_image'], name + ' route changed')
            require(digest(c / image) == item['selected_image_sha256'], name + ' image changed')
            grants = selected.get('key_value', []) if provider else [g for g in selected['grants'] if g['capability'] == 'storage.key-value']
            require(grants == item['kv_grants'], name + ' KV grants changed')
            if provider:
                shared = [g for g in grants if g['namespace'] in (1, 6)]
                require(shared == item['shared_key_grants'], name + ' shared-key audit changed')
                require(all(g['access'] == 'read' for g in shared), name + ' has a shared writer grant')
                internal = 'store/' + str(Path(name).parent / 'driver.elf')
                require(internal == item['internal_image'] and digest(c / internal) == item['internal_image_sha256'], name + ' internal fallback changed')
                if selected.get('image_source', {}).get('internal_fallback'):
                    require(item['selected_image_sha256'] == item['internal_image_sha256'], name + ' fallback differs')
            else:
                shared = sorted({g['instance_id'] for g in grants} & {1, 6})
                require(shared == item['shared_namespaces'], name + ' namespace coverage changed')
                require(set(item['selected_classification']) == {str(n) for n in shared}, name + ' missing classification')
                require((c / image).stat().st_size == item['selected_image_bytes'], name + ' image length changed')
                if 'build_receipt' in item:
                    receipt = read(c / 'receipts' / item['build_receipt'])
                    require(receipt.get('sha256', receipt.get('elf_sha256')) == item['selected_image_sha256'], name + ' rebuild receipt mismatch')
                else:
                    old = sd_seed.get(image[3:]) if image.startswith('SD/') else seed['store'].get(image[6:])
                    require(old == item['selected_image_sha256'], name + ' is not the recorded preserved seed image')
    source_commits = {item['repository']: item['commit'] for item in record['source_inputs']}
    for repository, revision in source_commits.items():
        actual = subprocess.check_output(['git', '-C', str(roots[repository]), 'rev-parse', 'HEAD'], text=True).strip()
        require(actual == revision, repository + ' source checkout revision changed')
    for item in record['source_inputs']:
        require(digest(roots[item['repository']] / item['path']) == item['sha256'], 'source changed: ' + item['repository'] + '/' + item['path'])
    for item in record['evidence_inputs']:
        require(digest(roots[item['root']] / item['path']) == item['sha256'], 'receipt changed: ' + item['path'])
    require(len(record['apps']) == 26 and len(record['providers']) == 32, 'selected count changed')
    require(sum(1 in a['shared_namespaces'] for a in record['apps']) == 20, 'namespace-1 count changed')
    require(sum(6 in a['shared_namespaces'] for a in record['apps']) == 5, 'namespace-6 count changed')


def negative_tests(record, roots):
    mutations = [
        lambda d: d['apps'].pop(),
        lambda d: d['providers'].pop(),
        lambda d: d['apps'][0]['kv_grants'].clear(),
        lambda d: d['apps'][0].update(selected_image_sha256='0' * 64),
        lambda d: d['source_inputs'][0].update(sha256='0' * 64),
        lambda d: d['evidence_inputs'][0].update(sha256='0' * 64),
        lambda d: d.update(shared_settings_enabled=True),
    ]
    for mutate in mutations:
        changed = copy.deepcopy(record)
        mutate(changed)
        try:
            validate(changed, roots)
        except ValueError:
            continue
        raise AssertionError('altered audit input was accepted')
    return len(mutations)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--inventory', type=Path, default=Path(__file__).with_name('WRITER_INVENTORY.json'))
    for name in ['composition', 'seed', 'system', 'utilities', 'productivity']:
        p.add_argument('--' + name, required=True, type=Path)
    p.add_argument('--negative-tests', action='store_true')
    a = p.parse_args()
    roots = {'composition': a.composition, 'seed': a.seed, 'System': a.system, 'Utilities': a.utilities, 'Productivity': a.productivity}
    record = read(a.inventory)
    validate(record, roots)
    print('PASS: 26 apps, 32 providers; 20 namespace-1 and 5 namespace-6 apps; exact grants, image routes/hashes, 72 source files and 16 evidence inputs')
    if a.negative_tests:
        print('PASS:', negative_tests(record, roots), 'altered/incomplete audit-input rejection cases')
    print('SOURCE AUDIT ONLY: preserved binary equivalence gaps remain; no activation or hardware qualification')


if __name__ == '__main__':
    main()

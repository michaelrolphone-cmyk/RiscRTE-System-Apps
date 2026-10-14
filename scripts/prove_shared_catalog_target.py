#!/usr/bin/env python3
"""Reproduce full-feature shared UI bytes using only public pinned inputs."""
import argparse
import hashlib
import json
import shlex
import subprocess
import tempfile
import io
import tarfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RECIPES = ROOT / 'test/native_apps/fixtures/catalog_target/recipes.json'
CURRENT = ROOT / 'test/native_apps/fixtures/catalog_target/t054-current.json'


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def materialize(repo, pin, destination):
    # Read immutable git objects, never an uncommitted dependency working tree.
    names = subprocess.check_output(
        ['git', 'ls-tree', '-r', '--name-only', pin, 'apps/clock', 'sdk', 'include'],
        cwd=repo, text=True).splitlines()
    if not names:
        raise ValueError('Pinned public bridge revision is unavailable')
    for name in names:
        path = Path(name)
        if path.suffix not in ('.h', '.inc') and path.name not in ('portable_navigation.c', 'portable_sleep.c'):
            continue
        out = destination / path
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_bytes(subprocess.check_output(['git', 'show', pin + ':' + name], cwd=repo))


def normalized_path(path, roots):
    for directory, prefix in roots:
        if path.is_relative_to(directory):
            return prefix + '/' + str(path.relative_to(directory))
    raise ValueError('Undeclared transitive dependency: ' + str(path))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True, type=Path)
    parser.add_argument('--bridges', required=True, type=Path,
                        help='Checkout containing the pinned public RiscRTE-Watch commit')
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'build/runtime-app-catalog/generic-target')
    parser.add_argument('--record-current', action='store_true', help='Intentionally refresh only the current-source fixture; never change baseline expectations')
    args = parser.parse_args()
    recipes = json.loads(RECIPES.read_text())
    assert recipes['schema'] == 1
    compiler = args.compiler.resolve()
    toolchain = compiler.parent.parent
    nm = str(compiler).removesuffix('gcc') + 'nm'
    version = subprocess.check_output([str(compiler), '--version'], text=True).splitlines()[0]
    assert version == recipes['compiler']['version'], version
    pin = recipes['shared_bridge']['revision']
    destination = args.output_dir.resolve()
    destination.mkdir(parents=True, exist_ok=True)
    results = []
    with tempfile.TemporaryDirectory(prefix='public-catalog-target-') as temporary:
        temporary = Path(temporary).resolve()
        bridge = temporary / 'bridges'
        materialize(args.bridges, pin, bridge)
        baseline = temporary / 'baseline'
        baseline.mkdir()
        archive = subprocess.check_output(['git', 'archive', recipes['baseline_system_revision'], 'Apps', 'lib', 'test/native_apps/fixtures'], cwd=ROOT)
        with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
            tar.extractall(baseline, filter='data')
        current = json.loads(CURRENT.read_text()) if CURRENT.exists() else {'schema': 1, 'profiles': {}}
        generated = {'schema': 1, 'profiles': {}}
        jobs = [('baseline', app, profile, baseline) for app, profile in recipes['profiles'].items()]
        for app, profile in recipes['profiles'].items():
            for atomic in (False, True):
                key = app + ('-atomic' if atomic else '-default')
                spec = dict(profile)
                spec['arguments'] = list(profile['arguments'])
                if atomic:
                    spec['arguments'] += ['-DPORTABLE_QUICK_ATOMIC_PREFERENCES']
                    if app == 'settings': spec['arguments'] += ['-DPORTABLE_SETTINGS_ATOMIC_EXPERIMENTAL']
                jobs.append(('current', key, spec, ROOT))
        for cohort, label, profile, system in jobs:
            app = label.split('-')[0]
            roots = ((system, 'System'), (bridge, 'SharedWatch'), (toolchain, 'Toolchain'))
            expected = profile if cohort == 'baseline' else current['profiles'].get(label)
            if cohort == 'current' and not args.record_current and expected is None:
                raise ValueError('Missing reviewed current-source fixture: ' + label)
            if cohort == 'current' and not args.record_current:
                assert expected['arguments'] == profile['arguments'], (label, 'profile arguments differ')
            builds = []
            for inventory in ('first', 'second'):
                out = temporary / (cohort + '-' + label + '-' + inventory)
                out.mkdir()
                (out / (app + '.map')).write_text('{ global: app_main; app_module_init; app_module_fini; local: *; };\n')
                (out / 'inventory.json').write_text(json.dumps({'installed': [inventory + '.elf'], 'available': inventory == 'first'}))
                (out / 'catalog.c').write_text('#error Compiled inventory must never be linked\n')
                substitutions = {'system': str(system), 'bridge': str(bridge), 'output': str(out)}
                command = [str(compiler), *[arg.format(**substitutions) for arg in profile['arguments']]]
                binary = out / (app + '.elf')
                subprocess.run([*command, '-o', str(binary)], check=True, timeout=180)
                digest = sha256(binary)
                if cohort == 'baseline' or not args.record_current:
                    assert digest == expected['expected_sha256'], (cohort, label, digest, expected['expected_sha256'])
                    assert binary.stat().st_size == expected['expected_size_bytes']
                all_symbols = subprocess.check_output([nm, str(binary)], text=True)
                assert not any(line.split()[-1] in ('portable_catalog', 'portable_catalog_count')
                               for line in all_symbols.splitlines() if line.split())
                dynamic = subprocess.check_output([nm, '-D', str(binary)], text=True)
                imports = sorted({line.split()[-1] for line in dynamic.splitlines() if ' U ' in ' ' + line})
                exports = sorted({line.split()[-1] for line in dynamic.splitlines()
                                  if len(line.split()) >= 3 and line.split()[-2] in ('T', 'D', 'B', 'R')})
                assert imports == profile['expected_imports'], imports
                assert exports == profile['expected_exports'], exports
                dependencies = subprocess.check_output([*command, '-M'], text=True, timeout=120)
                paths = {Path(token).resolve() for token in shlex.split(dependencies.replace('\\\n', ' '))
                         if not token.endswith(':') and Path(token).is_file()}
                inputs = {normalized_path(path, roots): sha256(path) for path in sorted(paths)}
                if cohort == 'baseline' or not args.record_current:
                    assert inputs == expected['expected_source_sha256'], (cohort, label, 'transitive source inputs differ')
                builds.append({'inventory': inventory, 'sha256': digest, 'size_bytes': binary.stat().st_size,
                               'imports': imports, 'exports': exports, 'arguments': profile['arguments'],
                               'source_sha256': inputs})
                if inventory == 'first':
                    (destination / (cohort + '-' + label + '.elf')).write_bytes(binary.read_bytes())
            assert builds[0]['sha256'] == builds[1]['sha256']
            assert builds[0]['source_sha256'] == builds[1]['source_sha256']
            if cohort == 'current':
                generated['profiles'][label] = {'expected_sha256': builds[0]['sha256'], 'expected_size_bytes': builds[0]['size_bytes'], 'expected_source_sha256': builds[0]['source_sha256'], 'arguments': profile['arguments']}
            results.append({'cohort': cohort, 'app': label, 'sha256': builds[0]['sha256'], 'inventory_independent': True,
                            'product_service_utilities_revision': profile['product_service_utilities_revision'],
                            'builds': builds})
    if args.record_current:
        CURRENT.write_text(json.dumps(generated, indent=2) + '\n')
    report = {'baseline_system_revision': recipes['baseline_system_revision'],
              'current_fixture_sha256': sha256(CURRENT),
              'current_fixture_recorded': args.record_current,
              'system_checkout_revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
              'system_app_sources_dirty': bool(subprocess.check_output(['git', 'diff', '--name-only', 'HEAD', 'Apps', 'lib'], cwd=ROOT, text=True).strip()),
              'proof_inputs_dirty': bool(subprocess.check_output(['git', 'status', '--porcelain', '--',
                  'scripts/prove_shared_catalog_target.py', 'test/native_apps/fixtures/catalog_target',
                  'test/native_apps/fixtures/watch_profile'], cwd=ROOT, text=True).strip()),
              'shared_bridge': recipes['shared_bridge'], 'compiler': version, 'recipe_sha256': sha256(RECIPES),
              'results': results, 'hardware_qualified': False, 'runtime_capability': 'app.catalog@1'}
    (destination / 'receipt.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'receipt': str(destination / 'receipt.json'),
                      'apps': [{key: result[key] for key in ('cohort', 'app', 'sha256')} for result in results]}, indent=2))


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Reproduce full-feature shared UI bytes using only public pinned inputs."""
import argparse
import hashlib
import json
import shlex
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RECIPES = ROOT / 'test/native_apps/fixtures/catalog_target/recipes.json'


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
        temporary = Path(temporary)
        bridge = temporary / 'bridges'
        materialize(args.bridges, pin, bridge)
        roots = ((ROOT, 'System'), (bridge, 'SharedWatch'), (toolchain, 'Toolchain'))
        for app, profile in recipes['profiles'].items():
            builds = []
            for inventory in ('first', 'second'):
                out = temporary / (app + '-' + inventory)
                out.mkdir()
                (out / (app + '.map')).write_text('{ global: app_main; app_module_init; app_module_fini; local: *; };\n')
                (out / 'inventory.json').write_text(json.dumps({'installed': [inventory + '.elf'], 'available': inventory == 'first'}))
                (out / 'catalog.c').write_text('#error Compiled inventory must never be linked\n')
                substitutions = {'system': str(ROOT), 'bridge': str(bridge), 'output': str(out)}
                command = [str(compiler), *[arg.format(**substitutions) for arg in profile['arguments']]]
                binary = out / (app + '.elf')
                subprocess.run([*command, '-o', str(binary)], check=True, timeout=180)
                digest = sha256(binary)
                assert digest == profile['expected_sha256'], (app, digest, profile['expected_sha256'])
                assert binary.stat().st_size == profile['expected_size_bytes']
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
                assert inputs == profile['expected_source_sha256'], {
                    'added': sorted(inputs.keys() - profile['expected_source_sha256'].keys()),
                    'removed': sorted(profile['expected_source_sha256'].keys() - inputs.keys()),
                    'changed': sorted(key for key in inputs.keys() & profile['expected_source_sha256'].keys()
                                      if inputs[key] != profile['expected_source_sha256'][key])}
                builds.append({'inventory': inventory, 'sha256': digest, 'size_bytes': binary.stat().st_size,
                               'imports': imports, 'exports': exports, 'arguments': profile['arguments'],
                               'source_sha256': inputs})
                if inventory == 'first':
                    (destination / (app + '.elf')).write_bytes(binary.read_bytes())
            assert builds[0]['sha256'] == builds[1]['sha256']
            assert builds[0]['source_sha256'] == builds[1]['source_sha256']
            results.append({'app': app, 'sha256': builds[0]['sha256'], 'inventory_independent': True,
                            'product_service_utilities_revision': profile['product_service_utilities_revision'],
                            'builds': builds})
    report = {'compiled_system_revision': recipes['baseline_system_revision'],
              'system_checkout_revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
              'system_app_sources_dirty': bool(subprocess.check_output(['git', 'diff', '--name-only', 'HEAD', 'Apps', 'lib'], cwd=ROOT, text=True).strip()),
              'proof_inputs_dirty': bool(subprocess.check_output(['git', 'status', '--porcelain', '--',
                  'scripts/prove_shared_catalog_target.py', 'test/native_apps/fixtures/catalog_target',
                  'test/native_apps/fixtures/watch_profile'], cwd=ROOT, text=True).strip()),
              'shared_bridge': recipes['shared_bridge'], 'compiler': version, 'recipe_sha256': sha256(RECIPES),
              'results': results, 'hardware_qualified': False, 'runtime_capability': 'app.catalog@1'}
    (destination / 'receipt.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'receipt': str(destination / 'receipt.json'),
                      'apps': [{key: result[key] for key in ('app', 'sha256')} for result in results]}, indent=2))


if __name__ == '__main__':
    main()

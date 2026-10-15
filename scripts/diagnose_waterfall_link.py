#!/usr/bin/env python3
"""Isolated, observational repeats of the pinned Wave Waterfall compiler command.

Does not change production inputs, flags, allocator settings, or resource limits.
Every failure and partial ELF is preserved. No output is used for a release.
"""
import argparse
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import platform
import resource
import shlex
import shutil
import subprocess
import tarfile
import time
from elftools.elf.elffile import ELFFile


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write_json(path, value):
    Path(path).write_text(json.dumps(value, indent=2) + '\n')


def capture(command):
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    return {'returncode': result.returncode, 'output': result.stdout}


def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def environment(compiler):
    binaries = {'gcc': compiler}
    for name in ('ld', 'cc1', 'collect2', 'lto-wrapper', 'lto1'):
        candidate = subprocess.check_output([compiler, '-print-prog-name=' + name], text=True).strip()
        found = Path(candidate) if Path(candidate).is_file() else Path(shutil.which(candidate) or candidate)
        if found.is_file():
            binaries[name] = found.resolve()
    plugin = Path(subprocess.check_output([compiler, '-print-file-name=liblto_plugin.so'], text=True).strip())
    if plugin.is_file():
        binaries['liblto_plugin.so'] = plugin.resolve()
    files = {}
    for name in ('/etc/os-release', '/proc/self/limits', '/proc/meminfo', '/proc/self/cgroup',
                 '/sys/fs/cgroup/memory.max', '/sys/fs/cgroup/memory.current',
                 '/sys/fs/cgroup/memory.events', '/sys/fs/cgroup/pids.max',
                 '/proc/sys/kernel/randomize_va_space'):
        path = Path(name)
        if path.is_file():
            files[name] = path.read_text()
    return {'cwd': str(Path.cwd()), 'uname': dict(platform.uname()._asdict()),
            'libc': platform.libc_ver(), 'files': files,
            'environment': {k: os.environ.get(k) for k in (
                'TMPDIR', 'TMP', 'TEMP', 'LANG', 'LC_ALL', 'SOURCE_DATE_EPOCH',
                'MALLOC_CHECK_', 'MALLOC_PERTURB_', 'GLIBC_TUNABLES', 'LD_PRELOAD',
                'LD_LIBRARY_PATH', 'GCC_EXEC_PREFIX', 'COMPILER_PATH', 'LIBRARY_PATH',
                'ImageOS', 'ImageVersion', 'RUNNER_OS', 'RUNNER_ARCH', 'GITHUB_SHA')},
            'binaries': {name: {'path': str(path), 'sha256': digest(path),
                                'ldd': capture(['ldd', str(path)])}
                         for name, path in binaries.items()},
            'compiler_version': capture([compiler, '--version']),
            'linker_version': capture([str(binaries['ld']), '--version']),
            'disk': capture(['df', '-Pk', '/tmp', str(Path.cwd())])}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--stage', required=True, type=Path)
    parser.add_argument('--source-cache', type=Path)
    parser.add_argument('--spec-dir', type=Path, default=Path(__file__).resolve().parents[1] / 'docs/shared-app-crashes/replay')
    parser.add_argument('--attempts', type=int, default=6, choices=range(1, 7))
    args = parser.parse_args()
    compiler = args.compiler.resolve()
    output, stage, spec_dir = args.output.resolve(), args.stage.resolve(), args.spec_dir.resolve()
    assert not output.exists(), 'Use a new evidence directory to preserve previous evidence'
    assert not stage.exists(), 'Use a new isolated staging directory'
    output.mkdir(parents=True)
    write_json(output / 'environment-before.json', environment(str(compiler)))
    spec = json.loads((spec_dir / 'inputs.json').read_text())
    recipe = json.loads((spec_dir / 'wave.json').read_text())['waterfall']
    inputs = {}
    for part in spec['input_parts']:
        inputs.update(json.loads((spec_dir / part).read_text()))
    cache = json.loads(args.source_cache.read_text()) if args.source_cache else {}
    paths = {'cc': compiler, 'cxx': Path(str(compiler)[:-3] + 'g++'), 'work': stage / 'work'}
    for key, row in spec['repositories'].items():
        target = stage / 'sources' / key
        paths[key] = target
        target.mkdir(parents=True)
        if key in cache:
            archive = subprocess.check_output(['git', '-C', cache[key], 'archive', row['commit']])
            tarfile.open(fileobj=io.BytesIO(archive)).extractall(target, filter='data')
        else:
            subprocess.run(['git', 'init', '-q', str(target)], check=True)
            subprocess.run(['git', '-C', str(target), 'fetch', '--depth=1',
                            'https://github.com/' + row['repository'] + '.git', row['commit']], check=True)
            subprocess.run(['git', '-C', str(target), 'checkout', '-q', '--detach', 'FETCH_HEAD'], check=True)
        (target / '.replay-commit').write_text(row['commit'] + '\n')

    def render(value):
        for key, path in sorted(paths.items(), key=lambda item: -len(item[0])):
            value = value.replace('@' + key + '@', str(path))
        return value

    for name, row in inputs.items():
        target = Path(render(name))
        if 'text' in row:
            assert hashlib.sha256(row['text'].encode()).hexdigest() == row['template_sha256']
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(render(row['text']))
    for name, row in inputs.items():
        if 'copy' in row:
            target = Path(render(name))
            target.parent.mkdir(parents=True, exist_ok=True)
            if row.get('symlink'):
                target.symlink_to(render(row['copy']))
            else:
                shutil.copyfile(render(row['copy']), target)
    verified, sdk = {}, {}
    for name, row in inputs.items():
        target = Path(render(name))
        if target.suffix == '.o':
            continue
        actual = digest(target)
        if 'text' not in row:
            assert actual == row['sha256'], name
        verified[name] = actual
        if row.get('symlink'):
            canonical = Path(render(row['copy']))
            assert target.resolve() == canonical.resolve() and target.resolve().is_relative_to(stage / 'sources')
            assert target.stat().st_ino == canonical.stat().st_ino and target.stat().st_dev == canonical.stat().st_dev
            sdk[name] = {'target': str(canonical), 'sha256': actual, 'same_inode': True}
    assert len(sdk) == 21
    command = [render(item) for item in recipe['command']]
    assert not any(item.endswith('.o') for item in command), 'Waterfall should have no prebuilt objects'
    target = Path(command[command.index('-o') + 1])
    target.parent.mkdir(parents=True, exist_ok=True)
    sources = [item for item in command if item.endswith('.c')]
    flags, index = [], 1
    while index < len(command):
        item = command[index]
        if item == '-o':
            index += 2
            continue
        if item not in sources and not item.startswith('-Wl,') and item != '-lgcc':
            flags.append(item)
        index += 1
    dependencies = set(sources)
    for source in sources:
        text = subprocess.check_output([str(compiler), *flags, '-MM', source], text=True)
        dependencies.update(shlex.split(text.replace('\\\n', ' ').split(':', 1)[1]))
    declared = {str(Path(render(name)).resolve()): value for name, value in verified.items()}
    closure = {str(Path(name).resolve()): digest(name) for name in sorted(dependencies)}
    assert all(declared.get(name) == value for name, value in closure.items())
    write_json(output / 'source-closure.json', {'repositories': spec['repositories'], 'inputs': verified,
               'sdk_symlinks': sdk, 'compiler_dependencies': closure,
               'spec_sha256': {p.name: digest(p) for p in sorted(spec_dir.iterdir()) if p.is_file()}})
    write_json(output / 'command.json', command)
    normalize = load_module('diagnose_normalize', spec_dir / 'normalize_xtensa_relocations.py').normalize
    compact = load_module('diagnose_compact', spec_dir / 'compact_current_elf.py').compact
    history = []
    for attempt in range(1, args.attempts + 1):
        # Only this diagnostic's output is removed; argv, sources, environment and limits stay identical.
        target.unlink(missing_ok=True)
        start = time.monotonic()
        process = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        prefix = output / ('attempt-%02d' % attempt)
        prefix.with_suffix('.log').write_bytes(process.stdout)
        row = {'attempt': attempt, 'returncode': process.returncode, 'seconds': time.monotonic() - start,
               'log_sha256': digest(prefix.with_suffix('.log')), 'command_sha256': digest(output / 'command.json'),
               'rusage_children': list(resource.getrusage(resource.RUSAGE_CHILDREN))}
        if target.is_file():
            raw = prefix.with_suffix('.raw.elf')
            shutil.copyfile(target, raw)
            row['raw_sha256'] = digest(raw)
            row['raw_bytes'] = raw.stat().st_size
        if process.returncode == 0:
            qualified = prefix.with_suffix('.qualified.elf')
            shutil.copyfile(target, qualified)
            normalize(qualified)
            compact(qualified, str(compiler), debug_path=prefix.with_suffix('.debug.elf'))
            row['qualified_sha256'] = digest(qualified)
            row['qualified_bytes'] = qualified.stat().st_size
            elf = ELFFile(io.BytesIO(qualified.read_bytes()))
            syms = elf.get_section_by_name('.dynsym')
            actual = {'imports': sorted(s.name for s in syms.iter_symbols() if s.name and s['st_shndx'] == 'SHN_UNDEF'),
                      'exports': sorted(s.name for s in syms.iter_symbols() if s.name and s['st_shndx'] != 'SHN_UNDEF')}
            row['qualified_exact'] = row['qualified_sha256'] == recipe['sha256'] and row['qualified_bytes'] == recipe['bytes'] and actual == recipe['symbols']
            assert row['qualified_exact'], row
        history.append(row)
        write_json(output / 'receipt.json', {'command': command, 'attempts': history,
                   'expected_sha256': recipe['sha256'], 'production_outputs_used': False})
        print(json.dumps(row), flush=True)
    write_json(output / 'environment-after.json', environment(str(compiler)))
    assert all(digest(Path(render(name))) == value for name, value in verified.items())
    write_json(output / 'summary.json', {'attempts': len(history),
               'successes': sum(row['returncode'] == 0 for row in history),
               'failures': sum(row['returncode'] != 0 for row in history),
               'all_successes_exact': all(row.get('qualified_exact', True) for row in history),
               'source_inputs_unchanged': True, 'source_input_count': len(verified),
               'sdk_symlink_count': len(sdk), 'compiler_dependency_count': len(closure)})


if __name__ == '__main__':
    main()

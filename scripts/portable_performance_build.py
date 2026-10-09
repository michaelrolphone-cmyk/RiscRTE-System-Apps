"""Explicit diagnostic Runtime SDK selection; never changes normal SDK pins."""
import hashlib
import json
import shutil
import subprocess
from pathlib import Path

RUNTIME_COMMIT = 'dfc0af505eb9a63092871f35e60ab2716a91d8f9'
DEFINE = '-DPORTABLE_PERFORMANCE_TRACE'
DISPLAY_DEFINE = '-DPORTABLE_PERFORMANCE_DISPLAY_METRICS'
DISPLAY_HEADERS = ('RiscDisplayOutputV1.h', 'RiscDisplayOutputPowerV1.h',
                   'RiscDisplayOutputMetricsV1.h')
VERSIONS = {'paper_clock': '0.3.7', 'springboard': '1.7.4', 'settings': '1.3.10'}


def options(parser):
    parser.add_argument('--performance-runtime-repo', type=Path,
                        help='Opt-in diagnostic trace build using immutable Runtime ' + RUNTIME_COMMIT)
    parser.add_argument('--performance-display-sdk', type=Path,
                        help='Optional diagnostic display SDK directory containing output, power and metrics headers')


def selected(args):
    return getattr(args, 'performance_runtime_repo', None) is not None


def validate(args, parser):
    if getattr(args, 'performance_display_sdk', None) and not selected(args):
        parser.error('--performance-display-sdk requires --performance-runtime-repo')


def read_display(args, parser):
    directory = getattr(args, 'performance_display_sdk', None)
    if directory is None:
        return None
    try:
        return {'source_directory': str(directory.resolve()),
                'headers': {name: (directory/name).read_bytes() for name in DISPLAY_HEADERS}}
    except OSError as error:
        parser.error('Cannot read diagnostic display SDK (all three headers required): ' + str(error))


def defines(receipt):
    return [DEFINE] + ([DISPLAY_DEFINE] if receipt.get('display_metrics') else [])


def read(args, parser):
    """Read the complete canonical SDK and license from Git, never the worktree."""
    repo = args.performance_runtime_repo
    try:
        paths = subprocess.check_output([
            'git', '-C', str(repo), 'ls-tree', '-r', '--name-only', RUNTIME_COMMIT, '--', 'sdk'
        ], stderr=subprocess.PIPE, text=True).splitlines()
        required = {'sdk/app/RiscRuntimeV1.h', 'sdk/app/RiscPerformanceV1.h',
                    'sdk/app/RiscRealtimeV1.h', 'sdk/app/RiscRetainedWakeV1.h'}
        if not required.issubset(paths):
            raise ValueError('Incomplete canonical diagnostic Runtime SDK')
        if any(not path.startswith('sdk/') or '..' in Path(path).parts for path in paths):
            raise ValueError('Invalid canonical diagnostic Runtime SDK path')
        return {path: subprocess.check_output([
            'git', '-C', str(repo), 'show', RUNTIME_COMMIT + ':' + path
        ], stderr=subprocess.PIPE) for path in [*paths, 'LICENSE']}
    except (OSError, subprocess.CalledProcessError, ValueError) as error:
        parser.error('Cannot read pinned diagnostic Runtime ' + RUNTIME_COMMIT + ': ' + str(error))


def app_sdk(source):
    return {Path(path).name: data for path, data in source.items()
            if path.startswith('sdk/app/') or path == 'LICENSE'}


def stage(root, out, source, base_includes=None, display=None, overrides=None):
    """Keep canonical SDK topology and overlay its headers beside quoted clients.

    A separate include path cannot replace old prefixes found by quoted includes
    in the portable clients. Use an isolated complete portable tree so a later
    normal build cannot accidentally reuse diagnostic headers.
    """
    directory = out/'performance-sdk'
    if directory.exists():
        shutil.rmtree(directory)
    includes = directory/'include'
    shutil.copytree(base_includes or root/'lib/PortableApps/include', includes)
    shutil.copytree(root/'lib/PortableApps/time', directory/'time')
    headers = {}
    for path, data in source.items():
        target = directory/path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        if path.startswith('sdk/') and path.endswith('.h'):
            name = Path(path).name
            if name in headers:
                raise ValueError('Ambiguous canonical SDK header: ' + name)
            (includes/name).write_bytes(data)
            headers[name] = hashlib.sha256(data).hexdigest()
    receipt = {
        'enabled': True, 'build_define': DEFINE,
        'repository': 'michaelrolphone-cmyk/RiscRTE', 'commit': RUNTIME_COMMIT,
        'source_sha256': {path: hashlib.sha256(data).hexdigest() for path, data in source.items()},
        'sdk_headers': headers,
        'app_source_sha256': {path: hashlib.sha256((root/path).read_bytes()).hexdigest() for path in (
            'lib/PortableApps/include/PortablePerformance.h', 'lib/PortableApps/src/performance.inc')},
        'builder_sha256': hashlib.sha256((root/'scripts/portable_performance_build.py').read_bytes()).hexdigest(),
    }
    if overrides:
        # Some deployments explicitly select a capability's extended SDK, such
        # as Clock's storage sleep suffix. Keep the immutable Runtime tree, but
        # attribute the actual compiled override to its separately selected file.
        override_dir = directory/'overrides'
        override_dir.mkdir()
        receipt['sdk_header_overrides'] = {}
        for name, path in overrides.items():
            data = path.read_bytes()
            (includes/name).write_bytes(data)
            (override_dir/name).write_bytes(data)
            digest = hashlib.sha256(data).hexdigest()
            receipt['sdk_headers'][name] = digest
            receipt['sdk_header_overrides'][name] = {'source_path': str(path.resolve()), 'sha256': digest}
    if display:
        display_dir = directory/'display'
        display_dir.mkdir()
        for name, data in display['headers'].items():
            (display_dir/name).write_bytes(data)
            (includes/name).write_bytes(data)
            receipt['sdk_headers'][name] = hashlib.sha256(data).hexdigest()
        receipt['display_metrics'] = {
            'enabled': True, 'build_define': DISPLAY_DEFINE,
            'source_directory': display['source_directory'],
            'sha256': {name: hashlib.sha256(data).hexdigest()
                       for name, data in display['headers'].items()},
        }
    notices = out/'licenses/performance-runtime'
    notices.mkdir(parents=True, exist_ok=True)
    (notices/'Runtime-LICENSE.txt').write_bytes(source['LICENSE'])
    (notices/'SOURCES.json').write_text(json.dumps(receipt, indent=2) + '\n')
    return includes, receipt

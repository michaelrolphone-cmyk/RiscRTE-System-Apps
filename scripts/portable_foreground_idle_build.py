"""Explicit ordinary-app returning Light selection; no deployment grants or writes.

This helper owns the selected compile inputs. Omitted flags leave legacy SDK
pins, include paths, definitions, sources and manifests unchanged. Physical
provider instances remain the deployment owner's decision.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path
import shutil
import subprocess
import tarfile
# This helper is also loaded by absolute path from external app repositories.
# Resolve its sibling by that path, never ambient PYTHONPATH/module-cache state.
import importlib.util
_sdk_spec = importlib.util.spec_from_file_location(
    __name__ + '_runtime_sdk_identity', Path(__file__).with_name('runtime_sdk_identity.py'))
_sdk_module = importlib.util.module_from_spec(_sdk_spec)
_sdk_spec.loader.exec_module(_sdk_module)
verify_runtime_sdk = _sdk_module.verify_runtime_sdk

RUNTIME_COMMIT = '273bdbccf65647a803ab0586a418f8283732baec'
SOURCE_NAMES = ('foreground_light_sleep.c', 'foreground_idle_preferences.c')
FLAGS = ('-DPORTABLE_FOREGROUND_IDLE', '-DPORTABLE_APP_SLEEP_LOCAL',
         '-DPORTABLE_NATIVE_CUSTODY_FENCE')
ALARM_READY_DRAIN_SUPPORTED = True
ALARM_READY_BUDGET_MS = 8
CAPABILITIES = ('power.sleep', 'display.power', 'input.power-key')
REQUIRED_HEADERS = ('PortableForegroundIdle.h',
                    'RiscRuntimeV1.h', 'RiscRuntimeSleepV1.h',
                    'RiscDisplayPowerV1.h', 'RiscPowerSleepV1.h',
                    'RiscPowerKeyNeutralizeV1.h', 'RiscKeyValueV1.h')
PATH_OPTIONS = ('foreground_idle_helper_dir', 'foreground_idle_runtime_dir',
                'foreground_idle_drivers_sdk_dir')


def idle_seconds(value):
    try:
        seconds = int(value)
    except (ValueError, TypeError) as error:
        raise argparse.ArgumentTypeError('idle seconds must be an integer from 5 to 3600') from error
    if not 5 <= seconds <= 3600:
        raise argparse.ArgumentTypeError('idle seconds must be from 5 to 3600')
    return seconds


def options(parser):
    parser.add_argument("--alarm-ready-drain", action="store_true",
                        help="Drain typed ready alarm phases within an 8 ms between-phase budget; requires tagged foreground alarm sleep")
    parser.add_argument("--foreground-alarm-sleep", action="store_true",
                        help="Explicit tagged API2 fresh-deadline returning Light protocol")
    parser.add_argument('--foreground-idle-helper-dir', type=Path,
                        help='Opt in to returning foreground Light; directory containing both generic helper C sources')
    parser.add_argument('--foreground-idle-runtime-dir', type=Path,
                        help='Clean Runtime descendant with identical SDK to ' + RUNTIME_COMMIT + '; headers read from that immutable commit')
    parser.add_argument('--foreground-idle-drivers-sdk-dir', type=Path,
                        help='Current Drivers sdk/driver directory with power.sleep, display.power and power-key contracts')
    parser.add_argument('--foreground-idle-fallback-seconds', type=idle_seconds,
                        help='Explicit missing-key fallback, 5–3600 seconds (15 for this opt-in candidate); valid saved sleep_idle wins')


def selected(args):
    return bool(getattr(args, PATH_OPTIONS[0], None))


def validate(args, parser):
    values = [getattr(args, name, None) for name in PATH_OPTIONS]
    fallback = getattr(args, 'foreground_idle_fallback_seconds', None)
    alarm = getattr(args, 'foreground_alarm_sleep', False)
    if getattr(args, 'alarm_ready_drain', False) and not alarm:
        parser.error('Alarm ready drain requires --foreground-alarm-sleep and its tagged client prerequisites')
    if alarm and (not all(values) or not getattr(args, 'alarm_client', False) or
                  not getattr(args, 'tagged_alarm_utilities', None)):
        parser.error('Foreground alarm sleep requires returning foreground Light and --alarm-client with --tagged-alarm-utilities')
    if not any(values) and fallback is None:
        return False
    if not all(values):
        parser.error('Foreground idle requires --foreground-idle-helper-dir, '
                     '--foreground-idle-runtime-dir and --foreground-idle-drivers-sdk-dir together')
    if fallback is not None:
        try:
            idle_seconds(fallback)
        except argparse.ArgumentTypeError as error:
            parser.error(str(error))
    if not getattr(args, 'navigation', False):
        parser.error('Foreground idle requires --navigation to retain the filtered power-key lifetime across helper grant release')
    for option in ('x4_idle_source', 'resident_shell_host', 'resident_shell_client',
                   'desk_lock_home', 'sparse_start'):
        if getattr(args, option, False):
            parser.error('Foreground idle is an independent ordinary-app policy; cannot combine with --' + option.replace('_', '-'))
    return True


def _sha(data):
    return hashlib.sha256(data).hexdigest()


def _repository(path):
    """Informational source identity; exact input bytes are always also hashed."""
    try:
        root = Path(subprocess.check_output(['git', '-C', str(path), 'rev-parse',
                                           '--show-toplevel'], text=True, stderr=subprocess.PIPE).strip())
        return {'root': str(root.resolve()),
                'commit': subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip(),
                'working_tree_dirty': bool(subprocess.check_output(
                    ['git', '-C', str(root), 'status', '--porcelain'], text=True).strip())}
    except (OSError, subprocess.CalledProcessError):
        return None


def _runtime_headers(repo, parser):
    try:
        verify_runtime_sdk(repo, RUNTIME_COMMIT)
        archive = subprocess.check_output(['git', '-C', str(repo), 'archive', RUNTIME_COMMIT,
                                          'sdk/app', 'sdk/driver', 'sdk/hardware', 'LICENSE'],
                                         stderr=subprocess.PIPE)
        with tarfile.open(fileobj=io.BytesIO(archive), mode='r:') as stream:
            return {member.name: stream.extractfile(member).read()
                    for member in stream.getmembers()
                    if member.isfile() and (member.name.endswith('.h') or member.name == 'LICENSE')}
    except (OSError, ValueError, subprocess.CalledProcessError, tarfile.TarError) as error:
        parser.error('Cannot read exact foreground-idle Runtime SDK: ' + str(error))


def runtime_app_sdk(args, parser, names):
    """Read a native profile subset from the explicitly selected canonical pin."""
    headers = _runtime_headers(Path(args.foreground_idle_runtime_dir).resolve(), parser)
    try:
        return {**{name: headers['sdk/app/' + name] for name in names},
                'LICENSE': headers['LICENSE']}
    except KeyError as error:
        parser.error('Canonical foreground-idle Runtime app SDK is missing ' + str(error))


def configure(args, parser, root, out, includes=None):
    """Return the actual include root, defines and additional source files.

    The SDK is staged together, not stacked using -I: quoted includes from
    Portable headers must resolve canonical Runtime/Drivers headers too.
    Drivers declarations override bundled SDK copies, then canonical Runtime
    driver/hardware/app headers override every shared ABI name in that order.
    """
    if not validate(args, parser):
        return includes, [], []
    root, out = Path(root).resolve(), Path(out).resolve()
    helper = Path(args.foreground_idle_helper_dir).resolve()
    runtime = Path(args.foreground_idle_runtime_dir).resolve()
    drivers = Path(args.foreground_idle_drivers_sdk_dir).resolve()
    sources = [helper / name for name in SOURCE_NAMES]
    missing = [str(path) for path in sources if not path.is_file()]
    if missing:
        parser.error('Missing foreground-idle helper sources: ' + ', '.join(missing))
    if not drivers.is_dir():
        parser.error('Missing foreground-idle Drivers SDK directory: ' + str(drivers))
    canonical = _runtime_headers(runtime, parser)
    base = Path(includes).resolve() if includes else root / 'lib/PortableApps/include'
    staged = out / 'foreground-idle-sdk/include'
    if staged == base or staged in base.parents or base in staged.parents:
        parser.error('Foreground idle output must not replace the selected source include tree')
    if staged.exists():
        shutil.rmtree(staged)
    shutil.copytree(base, staged)
    staged_time = staged.parent / 'time'
    if staged_time.exists():
        shutil.rmtree(staged_time)
    shutil.copytree(root / 'lib/PortableApps/time', staged_time)
    origin = {str(path.relative_to(staged)): {'source': str((base / path.relative_to(staged)).resolve()),
               'layer': 'system-base'} for path in staged.rglob('*') if path.is_file()}
    for source in sorted(drivers.rglob('*.h')):
        relative = str(source.relative_to(drivers))
        target = staged / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(source.read_bytes())
        origin[relative] = {'source': str(source.resolve()), 'layer': 'drivers-capability'}
    for layer in ('driver', 'hardware', 'app'):
        for name, data in sorted(canonical.items()):
            prefix = 'sdk/' + layer + '/'
            if not name.startswith(prefix):
                continue
            relative = name.removeprefix(prefix)
            target = staged / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
            origin[relative] = {'source': name, 'layer': 'runtime-' + layer, 'commit': RUNTIME_COMMIT}
    missing = [name for name in REQUIRED_HEADERS if not (staged / name).is_file()]
    if missing:
        parser.error('Missing foreground-idle staged contracts: ' + ', '.join(missing))
    seconds = getattr(args, 'foreground_idle_fallback_seconds', None)
    seconds = 15 if seconds is None else seconds
    flags = [*FLAGS, '-DPORTABLE_FOREGROUND_IDLE_FALLBACK_SECONDS=' + str(seconds) + 'u']
    alarm = getattr(args, 'foreground_alarm_sleep', False)
    if alarm:
        import portable_alarm_build
        tagged = portable_alarm_build.stage(args, parser, out, staged)
        for name in (portable_alarm_build.CLOCK_EDIT_HEADERS if getattr(args,'clock_edit_authority',False) else portable_alarm_build.HEADERS):
            origin[name] = {'source': 'lib/Alarm/include/' + name,
                            'layer': 'tagged-alarm', 'commit': tagged['commit']}
        flags.extend(('-DPORTABLE_FOREGROUND_ALARM_SLEEP', '-DALARM_SERVICE_TAGGED_V2',
                      '-DPORTABLE_ALARM_CLIENT'))
    if getattr(args, 'alarm_ready_drain', False):
        flags.extend(('-DPORTABLE_ALARM_READY_DRAIN',
                      '-DPORTABLE_ALARM_READY_BUDGET_MS=' + str(ALARM_READY_BUDGET_MS) + 'u'))
    if getattr(args,'clock_edit_authority',False):
        if not alarm: parser.error('Clock edit authority requires selected tagged foreground alarm')
        flags.append('-DALARM_SERVICE_CLOCK_EDIT_V2')
    for relative, item in origin.items():
        item['sha256'] = _sha((staged / relative).read_bytes())
    notices = out / 'licenses/foreground-idle'
    notices.mkdir(parents=True, exist_ok=True)
    (notices / 'Runtime-LICENSE.txt').write_bytes(canonical['LICENSE'])
    args.foreground_idle_receipt = {
        'schema': 1, 'enabled': True, 'mode': 'returning-light-only',
        **({'alarm_sleep': {'api': 2, 'protocol': 'fresh-ticket-v1', 'ticket_owner': 'adapter', 'duration_max_ms': 86400000}} if alarm else {}),
        **({'alarm_ready_drain': {'enabled': True, 'budget_ms': ALARM_READY_BUDGET_MS,
             'budget_scope': 'between synchronous phases, not a hard provider timeout',
             'requires': 'tagged alarm.service@2 step result', 'home_adoption': False}}
           if getattr(args, 'alarm_ready_drain', False) else {}),
        'fallback_seconds': seconds, 'fallback_applies': 'confirmed missing sleep_idle key only',
        'saved_valid_idle_preference_wins': True, 'invalid_or_unavailable_preference': 'defer idle',
        'sleep_mode_preference': 'not read or changed; Deep/Hybrid never selected',
        'preferences': {'capability': 'storage.key-value', 'api': 1, 'namespace': 1,
                        'key': 'sleep_idle', 'helper_operations': ['get'],
                        'grant_is_readonly': False, 'release_before_regular_app_work': True},
        'required_capabilities': [{'capability': name, 'api': 1} for name in CAPABILITIES],
        'physical_grant_selection': 'deployment-owned; not selected or applied by builder',
        'prerequisites': {'adapter_input_navigation': True,
            'key_lifetime': 'navigation grant remains live across Light and helper key-grant release; deployment navigation owns filtered power-key dependency'},
        'runtime_commit': RUNTIME_COMMIT, 'runtime_checkout': str(runtime),
        'runtime_sdk_identity': verify_runtime_sdk(runtime, RUNTIME_COMMIT),
        'sdk_verifier_sha256': _sha((Path(__file__).parent / 'runtime_sdk_identity.py').read_bytes()),
        'runtime_headers': 'immutable git object bytes; working-tree copies not used',
        'runtime_license_sha256': _sha(canonical['LICENSE']),
        'system_source': _repository(root), 'helper_source': _repository(helper),
        'drivers_source': _repository(drivers), 'defines': flags,
        'compiled_include_directory': str(staged),
        'sources': [str(path) for path in sources],
        'source_sha256': {str(path): _sha(path.read_bytes()) for path in sources},
        'builder_source_sha256': _sha(Path(__file__).read_bytes()),
        'header_origins': origin,
        'time_sha256': {str(path.relative_to(staged.parent / 'time')): _sha(path.read_bytes())
                        for path in sorted((staged.parent / 'time').rglob('*')) if path.is_file()},
        'hardware_qualification': False,
    }
    (notices / 'SOURCES.json').write_text(json.dumps(args.foreground_idle_receipt, indent=2) + '\n')
    return staged, flags, sources


def requirements(args, needs):
    if not selected(args):
        return
    if getattr(args, 'foreground_alarm_sleep', False):
        needs[:] = [row for row in needs if row['capability'] != 'alarm.service']
        needs.append({'capability': 'alarm.service', 'api': 2})
    for name in (*CAPABILITIES, 'storage.key-value'):
        item = {'capability': name, 'api': 1}
        if item not in needs:
            needs.append(item)


def record(args, value, flags, private_kv=(), compile_command=None):
    if not selected(args):
        return
    value['foreground_idle'] = args.foreground_idle_receipt
    value['build_defines'] = flags
    if compile_command is not None:
        value['compile_command'] = compile_command
        sources = [argument for argument in compile_command
                   if not argument.startswith('-') and Path(argument).suffix in ('.c', '.cc', '.cpp')]
        value['compile_inputs'] = {
            'sources': sources,
            'include_directories': [argument[2:] for argument in compile_command if argument.startswith('-I')],
            'definitions': [argument for argument in compile_command if argument.startswith('-D')],
            'source_sha256': {source: _sha(Path(source).read_bytes()) for source in sources},
        }
    bindings = value.setdefault('grant_bindings', {})
    existing = bindings.get('storage.key-value', [])
    if isinstance(existing, int):
        existing = [existing]
    # Namespace 1 is additive; never select wildcard 0 or rewrite private KV.
    bindings['storage.key-value'] = list(dict.fromkeys([*private_kv, *existing, 1]))
    if 0 in bindings['storage.key-value']:
        raise ValueError('Foreground idle requires exact KV namespaces, not instance 0')
    if 'required_grants' in value:
        grants = [item for item in value['required_grants'] if item['capability'] != 'storage.key-value']
        for item in grants:
            if item['capability'] in CAPABILITIES:
                item.pop('instance_id', None)
        grants += [{'capability': 'storage.key-value', 'api': 1, 'instance_id': namespace}
                   for namespace in bindings['storage.key-value']]
        value['required_grants'] = grants
        value['grant_count'] = len(grants)
    if 'mode_capabilities' in value:
        value['mode_capabilities'].update(automatic_idle_light=True, sleep_backend=True)

"""Explicit readonly native time profile for foreground System Apps.

Only selected builds stage the canonical SDK and tagged alarm headers. Legacy
builder input headers, versions, flags and link order are left alone.
"""
import hashlib
import json
import shutil
import subprocess
from pathlib import Path
import portable_alarm_build
import portable_performance_build

RUNTIME_COMMIT = '30dcec5ce6ce33223f2b203a2399283e1f758567'
SDK_HEADERS = ('RiscRuntimeV1.h', 'RiscRealtimeV1.h')
SOURCES = tuple('lib/PortableApps/src/' + name for name in (
    'PortableNativeTimeSource.c', 'PortableRealtimeClient.c', 'PortableTimeZone.c',
    'PortableTimeZoneCatalog.c', 'PortableTimeZonePreference.c'))


def options(parser):
    parser.add_argument('--time-profile', choices=['default', 'x4-native-time'], default='default',
                        help='Explicit native UTC/selected-IANA paper toolbar and tagged alarm API2')
    parser.add_argument('--native-time-runtime-repo', type=Path,
                        help='Local Runtime checkout containing pinned ' + RUNTIME_COMMIT)
    portable_alarm_build.options(parser)


def selected(args):
    return getattr(args, 'time_profile', 'default') == 'x4-native-time'


def validate(args, parser):
    portable_performance_build.validate(args, parser)
    native = selected(args)
    performance = portable_performance_build.selected(args)
    if performance and not native:
        parser.error('--performance-runtime-repo requires --time-profile x4-native-time')
    if args.display_rotation is None:
        args.display_rotation = 90 if native else 0
    if not native:
        if getattr(args, 'native_time_runtime_repo', None) or getattr(args, 'tagged_alarm_utilities', None):
            parser.error('Pinned native SDK options require --time-profile x4-native-time')
        return
    if (not args.native_time_runtime_repo and not performance) or not args.tagged_alarm_utilities:
        parser.error('x4-native-time requires --native-time-runtime-repo and --tagged-alarm-utilities')
    if not args.alarm_client:
        parser.error('x4-native-time requires --alarm-client (tagged API2)')
    if args.display_rotation != 90 or getattr(args, 'nova_ui', False):
        parser.error('x4-native-time requires portrait paper (--display-rotation 90, no --nova-ui)')
    if any(getattr(args, flag, False) for flag in ('wall_time', 'denver', 'quick_controls')):
        parser.error('x4-native-time cannot combine with legacy RTC or Watch quick-control policies')
    args.navigation = True


def configure(args, parser, root, out, app):
    if not selected(args):
        return root/'lib/PortableApps/include', [], [], None
    performance = None
    commit = RUNTIME_COMMIT
    if portable_performance_build.selected(args):
        if app != 'springboard':
            parser.error('Diagnostic native toolbar SDK selection is supported only for Springboard')
        source = portable_performance_build.read(args, parser)
        sdk = portable_performance_build.app_sdk(source)
        includes, performance = portable_performance_build.stage(
            root, out, source, display=portable_performance_build.read_display(args, parser))
        commit = portable_performance_build.RUNTIME_COMMIT
    else:
        try:
            sdk = {name: subprocess.check_output(['git', '-C', str(args.native_time_runtime_repo),
                   'show', RUNTIME_COMMIT + ':' + ('LICENSE' if name == 'LICENSE' else 'sdk/app/' + name)],
                   stderr=subprocess.PIPE) for name in (*SDK_HEADERS, 'LICENSE')}
        except (OSError, subprocess.CalledProcessError) as error:
            parser.error('Cannot read pinned native Runtime SDK: ' + str(error))
        includes = out/'native-time-sdk/include'
        shutil.copytree(root/'lib/PortableApps/include', includes, dirs_exist_ok=True)
        shutil.copytree(root/'lib/PortableApps/time', includes.parent/'time', dirs_exist_ok=True)
        for name in SDK_HEADERS:
            (includes/name).write_bytes(sdk[name])
    tagged = portable_alarm_build.stage(args, parser, out, includes)
    profile_path = 'lib/PortableApps/profiles/x4-native-time-' + app + '.json'
    profile = json.loads((root/profile_path).read_text())
    if performance:
        profile['version'] = portable_performance_build.version(app,args)
    elif getattr(args,'stage_logs',False):
        parts=profile['version'].split('.')
        parts[-1]=str(int(parts[-1])+1)
        profile['version']='.'.join(parts)
    runtime = {'repository': 'michaelrolphone-cmyk/RiscRTE', 'commit': commit,
               'sha256': {name: hashlib.sha256(data).hexdigest() for name, data in sdk.items()}}
    notices = out/'licenses/native-time'; notices.mkdir(parents=True, exist_ok=True)
    (notices/'Runtime-LICENSE.txt').write_bytes(sdk['LICENSE'])
    (notices/'SOURCES.json').write_text(json.dumps(runtime, indent=2) + '\n')
    shutil.copyfile(root/'lib/PortableApps/time/TIMEZONE_PROVENANCE.json', notices/'TIMEZONE-PROVENANCE.json')
    shutil.copyfile(root/'LICENSE', notices/'System-Apps-LICENSE.txt')
    receipt = dict(profile, native_time_sdk=runtime, tagged_alarm_sdk=tagged,
                   profile_source=profile_path)
    flags = ['-DPORTABLE_NATIVE_TIME_TOOLBAR', '-DPORTABLE_NATIVE_CUSTODY_FENCE',
             '-DALARM_SERVICE_TAGGED_V2']
    if performance:
        receipt['performance_trace'] = performance
        flags.extend(portable_performance_build.defines(performance,getattr(args,'stage_logs',False)))
    return includes, flags, [root/p for p in SOURCES], receipt


def requirements(args, needs):
    if not selected(args):
        return
    needs[:] = [item for item in needs if item['capability'] not in
                ('rtc.clock', 'runtime.realtime-control', 'alarm.service')]
    needs += [{'capability': name, 'api': api} for name, api in (
              ('alarm.service', 2), ('runtime.realtime', 1),
              ('storage.key-value', 1), ('board.battery', 1))]
    needs[:] = list({(item['capability'], item['api']): item for item in needs}.values())


def record(args, record, manifest, receipt):
    if receipt is None:
        return
    bindings = {'storage.key-value': [1], 'runtime.realtime': [0]}
    if manifest['id'] == 'wifi_settings':
        bindings.update({'net.wifi': [args.wifi_instance], 'storage.key-value': [6, 1]})
    if manifest['id'] == 'file_browser':
        bindings[args.storage_capability] = [args.storage_instance]
        if args.secondary_storage_instance is not None:
            bindings.setdefault('storage.volume', []).append(args.secondary_storage_instance)
    if args.quick_radios:
        bindings.setdefault('net.wifi', [15]); bindings['bluetooth.hci'] = [16]
    grants = [dict(item, instance_id=instance) for item in manifest['requires']
              for instance in bindings.get(item['capability'], [0])]
    record.update(receipt, time_profile='x4-native-time', clock_policy='native-utc-selected-iana',
                  time_policy='native-realtime-iana', required_grants=grants,
                  requested_capabilities=manifest['requires'], grant_count=len(grants),
                  grant_bindings=bindings, native_realtime_access='readonly', rtc_access='none',
                  preference_instance=1, time_zone_key='time_zone', publication='none')


def write_admission(root, out, manifest, record):
    if record.get('time_profile') != 'x4-native-time':
        return
    performance = record.get('performance_trace')
    includes = out/('performance-sdk/include' if performance else 'native-time-sdk/include')
    names = set((*SDK_HEADERS, *portable_alarm_build.HEADERS))
    if performance:
        names.update(performance['sdk_headers'])
        names.update(performance.get('display_metrics', {}).get('sha256', {}))
    headers = {name: hashlib.sha256((includes/name).read_bytes()).hexdigest()
               for name in sorted(names)}
    expected = dict(record['native_time_sdk']['sha256'], **record['tagged_alarm_sdk']['sha256'])
    if performance:
        expected.update(performance['sdk_headers'])
        expected.update(performance.get('display_metrics', {}).get('sha256', {}))
    assert all(headers[name] == expected[name] for name in headers)
    receipt = {'schema': 1, 'app': manifest['id'], 'version': manifest['version'],
               'source_repo': 'michaelrolphone-cmyk/RiscRTE-System-Apps',
               'source_revision': record['repository_commit'],
               'system_source_revision': record['repository_commit'],
               'runtime_source_revision': record['native_time_sdk']['commit'],
               'alarm_source_revision': portable_alarm_build.UTILITIES_COMMIT,
               'alarm_api': 2, 'time_policy': 'native-realtime-iana',
               'elf_sha256': record['sha256'], 'elf_bytes': record['size_bytes'],
               'requires': manifest['requires'], 'sdk_sha256': headers,
               'working_tree_dirty': record['working_tree_dirty']}
    if performance:
        receipt['performance_trace'] = performance
    (out/'x4-native-app.json').write_text(json.dumps(receipt, indent=2) + '\n')

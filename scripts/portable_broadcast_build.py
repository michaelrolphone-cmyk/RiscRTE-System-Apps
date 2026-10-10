"""Explicit native BLE telemetry selection shared by System app builders."""
import hashlib
import json
import portable_idle_build
import portable_paper_build
from pathlib import Path

VERSIONS = {'settings':'1.3.15', 'springboard':'1.7.7',
            'file_browser':'1.5.10', 'wifi_settings':'1.1.12',
            'ota_update':'1.2.2','app_store':'1.2.2'}
DEFINES = ['-DPORTABLE_BLE_BROADCAST', '-DPORTABLE_BLE_BROADCAST_DEFAULT_OFF',
           '-DPORTABLE_PAPER_PREFERENCES']
CAPABILITY = {'capability':'telemetry.broadcast', 'api':1}
SOURCES = ('scripts/portable_broadcast_build.py',
 'lib/PortableApps/include/PortableBroadcastClient.h',
 'lib/PortableApps/include/PortableBroadcastAppData.h',
 'lib/PortableApps/include/TelemetryBroadcastV1.h',
 'lib/PortableApps/include/RiscTelemetryV1.h',
 'lib/PortableApps/include/RiscBluetoothTelemetryV1.h',
 'lib/PortableApps/include/PortableRadioPolicy.h',
 'lib/PortableApps/include/PortableReaderPreferences.h',
 'lib/PortableApps/include/PortableTouch.h',
 'lib/PortableApps/src/paper.inc',
 'lib/PortableApps/src/broadcast_adapter.inc',
 'lib/PortableApps/src/adapter.c',
 'lib/PortableApps/src/native_custody_adapter.inc',
 'lib/PortableApps/src/PortableNativeTimeSource.c',
 'lib/PortableApps/src/foreground_adapter_open.inc',
 'lib/PortableApps/src/alarm.inc', 'lib/PortableApps/src/quick_adapter.inc')

def options(parser):
    parser.add_argument('--ble-broadcast', action='store_true',
                        help='Select native transient BLE telemetry client, off by default')

def selected(args):
    return getattr(args,'ble_broadcast',False)

def validate(args,parser,native):
    if not selected(args):return
    if not native:
        parser.error('--ble-broadcast requires the explicit x4-native-time profile')
    if not args.alarm_client or not getattr(args,'tagged_alarm_utilities',None):
        parser.error('--ble-broadcast requires --alarm-client and --tagged-alarm-utilities')

def flags(args):
    return list(DEFINES) if selected(args) else []

def version(args,app,baseline):
    return VERSIONS[app] if selected(args) else baseline

def requirements(args,needs):
    if not selected(args):return
    if CAPABILITY in needs:raise ValueError('Duplicate telemetry.broadcast declaration')
    pairs={(r['capability'],r['api']) for r in needs}
    if not {('storage.key-value',1),('alarm.service',2)}<=pairs:
        raise ValueError('Native telemetry requires shared preferences and tagged alarms')
    if len(pairs)!=len(needs) or any(type(r['api']) is not int for r in needs):
        raise ValueError('Invalid or duplicate native telemetry requirements')
    needs.append(dict(CAPABILITY))
    if len(needs)>16:raise ValueError('Telemetry declarations exceed Runtime capacity')

def record(args,root,record,manifest,defines):
    if not selected(args):return
    expected = portable_idle_build.version(args,manifest['id'],VERSIONS[manifest['id']])
    if manifest['id']=='settings' and getattr(args,'settings_list_scrolling',False):
        expected = '1.3.18'
    expected = portable_paper_build.touch_scroll_version(args,manifest['id'],expected)
    if getattr(args,'resident_shell_client',False):
        import portable_quick_build
        expected = portable_quick_build.version(args,manifest['id'],expected)
    if manifest['version']!=expected or manifest['requires'].count(CAPABILITY)!=1:
        raise ValueError('Telemetry manifest/version differs from selected builder')
    if not set(DEFINES)<=set(defines) or '-DPORTABLE_NATIVE_CUSTODY_FENCE' not in defines or '-DALARM_SERVICE_TAGGED_V2' not in defines:
        raise ValueError('Telemetry compilation lacks canonical native custody/alarm flags')
    grants=record.get('required_grants',[])
    if grants.count({**CAPABILITY,'instance_id':0})!=1 or len(grants)>16:
        raise ValueError('Telemetry singleton authority is missing or exceeds Runtime capacity')
    if not any(g=={'capability':'storage.key-value','api':1,'instance_id':1} for g in grants):
        raise ValueError('Telemetry shared namespace1 authority is missing')
    inputs=list(SOURCES)
    if manifest['id'] in ('ota_update','app_store'):inputs+=['Apps/update_portable.inc','lib/PortableApps/include/PortableUpdate.h','scripts/build_portable_updates.py']
    if manifest['id']=='wifi_settings':inputs+=['Apps/wifi_settings_portable.inc']
    if manifest['id']=='file_browser':inputs+=['Apps/file_browser_portable.inc']
    hashes={name:hashlib.sha256((root/name).read_bytes()).hexdigest() for name in inputs}
    record.setdefault('source_sha256',{}).update(hashes)
    record['build_defines']=list(defines)
    record['ble_broadcast']={'enabled':True,'default':'off','grant_lifetime':'transient',
        'capability':'telemetry.broadcast','api':1,'instance_id':0,
        'shared_preferences_instance':1,'foreground_excluded':False,'reader_flip_ui':True,
        'source_sha256':hashes,'hardware_verified':False}

def admission_fields(record,receipt):
    if not record.get('ble_broadcast'):return
    receipt['ble_broadcast']=record['ble_broadcast']
    receipt['build_defines']=record['build_defines']
    receipt['required_grants']=record['required_grants']
    receipt['grant_count']=len(record['required_grants'])

def write_settings_admission(args,root,out,includes,manifest,record):
    if not selected(args):return
    expected=dict(record['native_time_sdk_headers'])
    performance=record.get('performance_trace')
    if performance:
        expected.update(performance['sdk_headers'])
        expected.update(performance.get('display_metrics',{}).get('sha256',{}))
    expected.update({name:digest for name,digest in record['tagged_alarm_sdk']['sha256'].items() if name!='LICENSE'})
    idle=record.get('idle_policy')
    if idle:
        includes=Path(idle['compiled_include_directory'])
        expected.update(idle['sdk_sha256'])
    resident=record.get('resident_shell')
    if resident:expected.update(resident['sdk_sha256'])
    headers={name:hashlib.sha256((includes/name).read_bytes()).hexdigest() for name in expected}
    if headers!=expected:raise ValueError('Compiled Settings telemetry SDK differs from custody')
    receipt={'schema':1,'app':'settings','version':manifest['version'],
        'source_repo':'michaelrolphone-cmyk/RiscRTE-System-Apps',
        'source_revision':record['repository_commit'],'system_source_revision':record['repository_commit'],
        'runtime_source_revision':record['native_time_runtime_commit'],
        'alarm_source_revision':record['tagged_alarm_sdk']['commit'],'alarm_api':2,
        'time_policy':'native-realtime-iana','elf_sha256':record['sha256'],
        'elf_bytes':record['size_bytes'],'requires':manifest['requires'],'sdk_sha256':headers,
        'working_tree_dirty':record['working_tree_dirty'],
        'build_record_sha256':hashlib.sha256((out/'settings-build-record.json').read_bytes()).hexdigest()}
    admission_fields(record,receipt)
    for key in ('paper_motion','touch_scrolling','home_desk_lock','performance_trace','idle_policy','resident_shell'):
        if key in record:receipt[key]=record[key]
    (out/'x4-native-app.json').write_text(json.dumps(receipt,indent=2)+'\n')

#!/usr/bin/env python3
"""Build the shared Settings app against the minimal capability client.

Development artifact only. The deployment supplies display.output@1,
input.touch.raw@1, rtc.clock@2 and namespace-1 storage.key-value@1 grants; no additional runtime exports.
"""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import portable_quick_build
import portable_alarm_build
import portable_performance_build
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
NATIVE_TIME_RUNTIME_COMMIT = '602ae9bd618e13407b5b94bcad86cdabc23c99ea'
NATIVE_TIME_SDK_HEADERS = ('RiscRuntimeV1.h', 'RiscRealtimeV1.h')
NATIVE_TIME_SOURCES = tuple('lib/PortableApps/src/'+name for name in (
    'PortableSetTime.c', 'PortableRealtimeClient.c', 'PortableTimeZone.c',
    'PortableTimeZoneCatalog.c', 'PortableTimeZonePreference.c'))
NATIVE_TIME_CONTROLLER = ('lib/PortableApps/src/settings_native_time.inc',
    'lib/PortableApps/src/native_custody_adapter.inc',
    'lib/PortableApps/include/PortableNativeCustody.h')


def native_time_sdk(args, parser):
    """Read canonical SDK/license bytes from the selected immutable source.

    A checkout's current branch and dirty files cannot silently replace the
    Runtime contract used by this opt-in profile. No fetch or SDK vendoring.
    """
    repo = getattr(args, 'native_time_runtime_repo', None)
    if not repo:
        parser.error('x4-native-time requires --native-time-runtime-repo')
    try:
        return {name: subprocess.check_output(['git', '-C', str(repo), 'show',
            NATIVE_TIME_RUNTIME_COMMIT+':'+('LICENSE' if name == 'LICENSE' else 'sdk/app/'+name)],
            stderr=subprocess.PIPE) for name in (*NATIVE_TIME_SDK_HEADERS, 'LICENSE')}
    except (OSError, subprocess.CalledProcessError) as error:
        parser.error('Cannot read canonical Runtime '+NATIVE_TIME_RUNTIME_COMMIT+
                     ' SDK and LICENSE from --native-time-runtime-repo: '+str(error))


def stage_native_time_sdk(out, sdk):
    # Quoted includes must resolve to the same staged canonical Runtime prefix.
    # A second include directory alone would leave bundled headers on the old
    # prefix. Preserve relative ../time includes alongside the complete tree.
    includes = out/'native-time-sdk/include'
    shutil.copytree(ROOT/'lib/PortableApps/include', includes, dirs_exist_ok=True)
    shutil.copytree(ROOT/'lib/PortableApps/time', includes.parent/'time', dirs_exist_ok=True)
    for name in NATIVE_TIME_SDK_HEADERS:
        (includes/name).write_bytes(sdk[name])
    return includes

def build(args,parser=None):
    parser=parser or argparse.ArgumentParser(description=__doc__)
    portable_performance_build.validate(args,parser)
    cc = os.environ.get('NATIVE_APP_CC') or shutil.which('xtensa-esp32s3-elf-gcc')
    if not cc:
        core = Path(os.environ.get('PLATFORMIO_CORE_DIR', Path.home()/'.platformio'))
        cc = str(core/'packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc')
    out = args.output_dir or ROOT/'dist/portable'
    profile=getattr(args,'settings_profile','default')
    native_time=profile=='x4-native-time'
    performance_source=None
    performance=None
    runtime_commit=NATIVE_TIME_RUNTIME_COMMIT
    if portable_performance_build.selected(args) and not native_time:
        parser.error('--performance-runtime-repo requires --settings-profile x4-native-time')
    desk_clock=profile in ('x4-desk-clock','x4-native-time')
    if args.display_rotation is None: args.display_rotation=90 if native_time else 0
    sdk=None
    if getattr(args,'tagged_alarm_utilities',None) and not native_time:
        parser.error('Tagged alarm Settings requires --settings-profile x4-native-time')
    if native_time:
        if args.display_rotation!=90 or args.nova_ui:
            parser.error('x4-native-time requires the portrait paper profile (--display-rotation 90, no --nova-ui)')
        if args.denver or args.wall_time:
            parser.error('x4-native-time cannot use --denver or --wall-time RTC policies')
        args.navigation=True
        if portable_performance_build.selected(args):
            performance_source=portable_performance_build.read(args,parser)
            sdk=portable_performance_build.app_sdk(performance_source)
            runtime_commit=portable_performance_build.RUNTIME_COMMIT
        else:
            sdk=native_time_sdk(args,parser)
        missing=[p for p in (*NATIVE_TIME_SOURCES,*NATIVE_TIME_CONTROLLER) if not (ROOT/p).is_file()]
        if missing: parser.error('Missing native-time Settings implementation: '+', '.join(missing))
    elif getattr(args,'native_time_runtime_repo',None):
        parser.error('--native-time-runtime-repo requires --settings-profile x4-native-time')
    flags=['-DPORTABLE_SETTINGS_APP','-DPORTABLE_DISPLAY_ROTATION='+str(args.display_rotation)]
    if args.return_app:
        if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*\.elf',args.return_app):
            raise ValueError('Return app must be a plain .elf filename')
        flags.append('-DPORTABLE_RETURN_APP=\"'+args.return_app+'\"')
    if args.sleep_settings or desk_clock: flags.append('-DPORTABLE_SLEEP_SETTINGS')
    if desk_clock: flags.append('-DPORTABLE_SETTINGS_X4_DESK_CLOCK')
    if native_time:
        flags+=['-DPORTABLE_SETTINGS_NATIVE_TIME','-DPORTABLE_SETTINGS_TIME_ZONE',
                '-DPORTABLE_NATIVE_CUSTODY_FENCE']
    if args.alarm_settings: flags.append('-DPORTABLE_ALARM_SETTINGS')
    if args.nova_ui: flags.append('-DPORTABLE_NOVA_UI')
    if args.alarm_client: flags.append('-DPORTABLE_ALARM_CLIENT')
    if args.denver and args.wall_time:parser.error('Choose one explicit RTC policy')
    if args.denver: flags.append('-DPORTABLE_RTC_UTC8_DENVER')
    if args.wall_time:flags.append('-DPORTABLE_RTC_WALL_TIME')
    if args.full_frames: flags.append('-DPORTABLE_FORCE_FULL_FRAMES')
    if args.navigation: flags.append('-DPORTABLE_INPUT_NAVIGATION')
    flags.append('-DPORTABLE_TOUCH_ROTATION='+str(args.touch_rotation))
    version_path=('lib/PortableApps/profiles/x4-native-time-settings.json' if native_time else
                  'lib/PortableApps/profiles/x4-desk-clock-settings.json' if desk_clock else 'Apps/settings.json')
    version=json.loads((ROOT/version_path).read_text())['version']
    if getattr(args,'tagged_alarm_utilities',None):version='1.3.9'
    if performance_source:version=portable_performance_build.VERSIONS['settings']
    flags.append('-DPORTABLE_SETTINGS_VERSION=\"'+version+'\"')
    out.mkdir(parents=True, exist_ok=True)
    if performance_source:
        includes,performance=portable_performance_build.stage(ROOT,out,performance_source,
            display=portable_performance_build.read_display(args,parser))
        flags.extend(portable_performance_build.defines(performance))
    else:
        includes=stage_native_time_sdk(out,sdk) if native_time else ROOT/'lib/PortableApps/include'
    tagged_alarm=portable_alarm_build.stage(args,parser,out,includes)
    if tagged_alarm:flags.append('-DALARM_SERVICE_TAGGED_V2')
    quick_flags,quick_sources=portable_quick_build.configure(args,parser,ROOT,out);flags+=quick_flags
    exports = {'app_main', 'app_module_init', 'app_module_fini'}
    mapping = out/'settings.map'
    mapping.write_text('{ global: '+ '; '.join(sorted(exports))+'; local: *; };\n')
    catalog = out/'catalog.c'
    catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
    elf = out/'settings.elf'
    entry = 'Apps/settings_native_entry.c' if native_time else 'Apps/settings.c'
    sources = [ROOT/entry, ROOT/'lib/PortableApps/src/adapter.c', catalog]+quick_sources
    if native_time: sources += [ROOT/p for p in NATIVE_TIME_SOURCES]
    subprocess.run([cc, '-std=c11', '-Os', '-fPIC', '-mtext-section-literals', '-mlongcalls',
        '-fvisibility=hidden', '-ffreestanding', '-fno-builtin', '-nostdlib', '-nostartfiles', '-shared',
        '-Wl,--hash-style=sysv', '-Wl,--version-script='+str(mapping), '-Wall', '-Wextra', '-Werror',
        *flags, '-I'+str(includes),
        '-I'+str(ROOT/'lib/NativeApps/include'), *map(str, sources), '-o', str(elf)], check=True, timeout=120)
    symbols = subprocess.check_output([cc.removesuffix('gcc')+'nm', '-D', str(elf)], text=True)
    imports = {line.split()[-1] for line in symbols.splitlines() if ' U ' in ' '+line}
    allowed = {'risc_runtime_get_api','memcpy','memset','memcmp','strcmp','strlen','snprintf','strcpy','malloc','free'}
    if not imports <= allowed: raise ValueError('Unexpected imports: '+str(imports-allowed))
    actual = {line.split()[-1] for line in symbols.splitlines() if len(line.split())>=3 and line.split()[-2] in ('T','D','B','R')}
    if actual != exports: raise ValueError('Unexpected exports: '+str(actual))
    data = elf.read_bytes()
    if data[:7] != b'\x7fELF\x01\x01\x01' or data[16:20] != b'\x03\x00\x5e\x00':
        raise ValueError('Expected ELF32 little-endian Xtensa ET_DYN')
    validator = ROOT/'build/portable-validate-elf'
    validator.parent.mkdir(exist_ok=True)
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',
        '-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),
        str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'),str(ROOT/'test/native_apps/validate_test.c'),
        '-o',str(validator)],check=True,timeout=60)
    subprocess.run([str(validator),str(elf)],check=True,timeout=60)
    manifest={'type':'application','id':'settings','version':version,'architecture':'xtensa-esp32s3',
        'file_name':'settings.elf','entry':'app_main','requires':[
            {'capability':'display.output','api':1}, {'capability':'input.touch.raw','api':1},
            {'capability':'rtc.clock','api':2}]}
    if args.alarm_client: manifest['requires'].append({'capability':'alarm.service','api':2 if tagged_alarm else 1})
    manifest['requires'].append({'capability':'storage.key-value','api':1})
    if native_time: manifest['requires'] += [{'capability':'runtime.realtime-control','api':1},
                                              {'capability':'board.battery','api':1}]
    if args.navigation: manifest['requires'].append({'capability':'input.navigation','api':1})
    portable_quick_build.requirements(args,manifest['requires'])
    (out/'settings.json').write_text(json.dumps(manifest,indent=2)+'\n')
    inputs=['Apps/settings.c','Apps/settings.json','lib/PortableApps/src/adapter.c',
            'lib/PortableApps/src/settings.inc','lib/PortableApps/include/PortableRtcClock.h',
            'lib/PortableApps/RTC_PROVENANCE.json','lib/PortableApps/SOURCES.json']
    inputs += ['lib/PortableApps/include/'+name for name in json.loads((ROOT/'lib/PortableApps/SOURCES.json').read_text())]
    inputs += [str(p.relative_to(ROOT)) for root in ['lib/PortableApps/time','lib/PortableApps/settings_fonts'] for p in sorted((ROOT/root).rglob('*')) if p.is_file()]
    inputs += ['lib/PortableApps/src/settings_view.inc','lib/PortableApps/include/PortableTime.h']
    inputs += ['lib/PortableApps/include/PortableApps.h','lib/PortableApps/include/PortableTouch.h',
               'lib/NativeApps/include/T5AppApi.h','lib/NativeApps/include/T5UiApi.h',
               'lib/NativeApps/include/T5StorageApi.h','lib/NativeApps/include/T5VideoApi.h']
    # Include all shared-client inputs: the adapter can also carry the shared
    # Springboard presentation/fonts after the coordinated integration.
    inputs += [str(p.relative_to(ROOT)) for p in sorted((ROOT/'lib/PortableApps').rglob('*')) if p.is_file()]
    if (ROOT/'Apps/SpringboardPresentation.h').exists(): inputs.append('Apps/SpringboardPresentation.h')
    inputs += ['Apps/PaperPresentation.h','Apps/PaperFrame.h']
    if tagged_alarm:inputs.append('scripts/portable_alarm_build.py')
    inputs += ['scripts/build_portable_settings.py',version_path]
    if performance: inputs.append('scripts/portable_performance_build.py')
    if native_time: inputs += ['LICENSE','Apps/settings_native_entry.c']
    inputs=sorted(set(inputs))
    for group in ['settings_fonts','fonts','paper_fonts']:
        source=ROOT/'lib/PortableApps'/group
        if source.exists():
            destination=out/'licenses'/group;destination.mkdir(parents=True,exist_ok=True)
            for notice in [*source.glob('LICENSE-*.txt'),*source.glob('SOURCES.json')]:
                shutil.copyfile(notice,destination/notice.name)
    destination=out/'licenses'/'portable-settings';destination.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(ROOT/'LICENSE',destination/'System-Apps-LICENSE.txt')
    shutil.copyfile(ROOT/'lib/PortableApps/time/SOURCES.json',destination/'time-SOURCES.json')
    shutil.copyfile(ROOT/'lib/PortableApps/RTC_PROVENANCE.json',destination/'RTC-PROVENANCE.json')
    if native_time:
        shutil.copyfile(ROOT/'lib/PortableApps/time/TIMEZONE_PROVENANCE.json',destination/'TIMEZONE-PROVENANCE.json')
        (destination/'Runtime-LICENSE.txt').write_bytes(sdk['LICENSE'])
        provenance={'repository':'michaelrolphone-cmyk/RiscRTE',
            'commit':runtime_commit,
            'source_sha256':{'LICENSE' if name=='LICENSE' else 'sdk/app/'+name:
                hashlib.sha256(data).hexdigest() for name,data in sdk.items()}}
        (destination/'native-time-SDK-SOURCES.json').write_text(json.dumps(provenance,indent=2)+'\n')
    record={'purpose':'portable-development-artifact-not-deployment','version':version,
        'repository_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
        'working_tree_dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True).strip()),
        'compiler':subprocess.check_output([cc,'--version'],text=True).splitlines()[0],
        'sha256':hashlib.sha256(data).hexdigest(),'size_bytes':len(data),
        'imports':sorted(imports),'exports':sorted(exports),
        'settings_profile':profile,'sleep_modes':(['light','deep'] if desk_clock else ['light','deep','hybrid']) if args.sleep_settings or desk_clock else [],
        'sleep_fallback':'light' if desk_clock else 'hybrid','desk_clock_faces':['Segments','Sans','Serif','Minimal','Railway','Deco'] if desk_clock else [],
        'preferences':{'instance':1,'sleep_key':'sleep_mode','face_key':'desk_clock_face' if desk_clock else None},
        'build_defines':flags,'time_policy':'rtc-utc8-to-America-Denver' if args.denver else 'identity-raw',
        'home_app':args.home_app,'quick_actions':args.quick_actions,'quick_radios':args.quick_radios,'return_app':args.return_app,'display_rotation':args.display_rotation,'full_frames':args.full_frames,'navigation':args.navigation,'touch_rotation':args.touch_rotation,
        'source_sha256':{p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in inputs}}
    if tagged_alarm:record['tagged_alarm_sdk']=tagged_alarm
    if performance:record['performance_trace']=performance
    if native_time:
        record.update(time_policy='native-realtime-iana',invocation_retention=True,
            native_time_runtime_commit=runtime_commit,
            native_time_sdk_headers={name:hashlib.sha256(sdk[name]).hexdigest()
                for name in (sorted(set(sdk)-{'LICENSE'}) if performance else NATIVE_TIME_SDK_HEADERS)},
            native_time_runtime_license_sha256=hashlib.sha256(sdk['LICENSE']).hexdigest(),
            native_time_control_instance=0,rtc_access='explicit-save-only',
            grant_count=len(manifest['requires']),
            required_grants=[dict(requirement,instance_id={'storage.key-value':1,'net.wifi':15,
                'bluetooth.hci':16}.get(requirement['capability'],0)) for requirement in manifest['requires']],
            mode_capabilities={'foreground_settings':True,'native_realtime_read':True,
                'explicit_checked_set_time':True,'timezone_preference_only':True,
                'sleep_preference_modes':['light','deep'],'sleep_backend':False,
                'alarm_overlay':bool(args.alarm_client),'quick_actions':bool(args.quick_actions),
                'quick_radios':bool(args.quick_radios)})
        record['preferences'].update(time_zone_key='time_zone',rtc_basis_key='rtc_basis',
            flip_ui_key='reader_flip_ui',language_key='reader_language')
    (out/'settings-build-record.json').write_text(json.dumps(record,indent=2)+'\n')
    print('Portable Settings: target layout, ELF validator, import/export checks passed')

def argument_parser():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--settings-profile',choices=['default','x4-desk-clock','x4-native-time'],default='default',help='Opt-in paper Settings profiles; do not qualify a sleep backend')
    parser.add_argument('--native-time-runtime-repo',type=Path,help='Local Runtime Git checkout containing canonical '+NATIVE_TIME_RUNTIME_COMMIT+' SDK; x4-native-time only')
    parser.add_argument('--display-rotation',type=int,choices=[0,90],default=None,help='Default: 90 for x4-native-time, otherwise 0')
    parser.add_argument('--nova-ui',action='store_true',help='Settings-derived 240x240 Nova utility profile')
    parser.add_argument('--alarm-client',action='store_true',help='Explicit alarm.service foreground overlay consumer')
    parser.add_argument('--alarm-settings',action='store_true',help='Explicit namespace-1 alert mode choice')
    parser.add_argument('--sleep-settings',action='store_true',help='Enable explicit namespace-1 sleep choice; requires storage.key-value@1 grant')
    parser.add_argument('--denver',action='store_true',help='Explicit deployment policy: RTC fixed UTC+08, display America/Denver')
    parser.add_argument('--full-frames',action='store_true',help='Disable optional partial-damage and previous-frame cache')
    parser.add_argument('--navigation',action='store_true',help='Require a granted input.navigation@1 provider')
    parser.add_argument('--touch-rotation',type=int,choices=[0,180],default=0)
    parser.add_argument('--return-app',help='Explicit root Back destination as a plain .elf filename')
    parser.add_argument('--output-dir',type=Path)
    parser.add_argument('--wall-time',action='store_true',help='Explicit unchanged RTC wall-time policy')
    portable_quick_build.options(parser)
    portable_alarm_build.options(parser)
    portable_performance_build.options(parser)
    return parser


if __name__ == '__main__':
    parser=argument_parser()
    build(parser.parse_args(),parser)

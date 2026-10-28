#!/usr/bin/env python3
"""Build the shared Wi-Fi Settings app against the minimal capability client.

Development artifact only. The deployment supplies display.output@1,
input.touch.raw@1, net.wifi@1 and namespace-6 storage.key-value@1 grants; no additional runtime exports.
"""
from native_toolchain import compiler_path as resolve_compiler
import argparse
import hashlib
import json
import os
import re
import portable_broadcast_build
import portable_quick_build
import portable_idle_build
import portable_foreground_idle_build
import portable_native_toolbar_build
import portable_paper_build
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
# Complete dependency closure of the shared sampler and native clock client.
# The clock client references timezone conversion even when this workflow seeds UTC.
TIME_SYNC_SOURCES = ('PortableTimeSync.c', 'PortableRealtimeClient.c',
                     'PortableTimeZone.c', 'PortableTimeZoneCatalog.c')

def build(args, parser=None):
    parser = parser or argparse.ArgumentParser(description=__doc__)
    time_sync=getattr(args,'time_server_sync',False)
    if time_sync and (not getattr(args,'foreground_alarm_sleep',False) or not getattr(args,'clock_edit_authority',False)):
        parser.error('--time-server-sync requires --foreground-alarm-sleep and --clock-edit-authority')
    server=getattr(args,'time_server','pool.ntp.org')
    if time_sync and (not re.fullmatch(r'[A-Za-z0-9](?:[A-Za-z0-9.-]{0,251}[A-Za-z0-9])?',server)):
        parser.error('--time-server must be a bounded DNS name or IPv4 address')
    native_ui=getattr(args,'watch_native_ui',False)
    if native_ui:
        if (args.display_rotation not in (None,0) or args.touch_rotation != 0 or args.nova_ui or
            args.paper_transitions or args.resident_shell_client or getattr(args,'touch_scrolling',False) or
            getattr(args,'shared_text_input',False) or portable_native_toolbar_build.selected(args)):
            parser.error('--watch-native-ui requires unrotated Watch local keyboard, without legacy Nova/paper/resident/native-time profiles')
        args.display_rotation=0
        args.safe_viewport=True
    portable_foreground_idle_build.validate(args,parser)
    portable_native_toolbar_build.validate(args,parser)
    portable_broadcast_build.validate(args,parser,portable_native_toolbar_build.selected(args))
    atomic=getattr(args,'experimental_atomic_wifi',False)
    availability=getattr(args,'experimental_settings_availability',False)
    if availability and not atomic:
        parser.error('--experimental-settings-availability requires --experimental-atomic-wifi')
    if atomic and (not portable_native_toolbar_build.selected(args) or not args.resident_shell_client):
        parser.error('--experimental-atomic-wifi requires explicit X4 native-time resident selection')
    policy=getattr(args,'shared_text_policy',False)
    if policy and (not availability or not args.resident_policy):
        parser.error('--shared-text-policy requires X4 atomic settings availability and resident policy')
    scrolling=getattr(args,'touch_scrolling',False)
    shared_text=getattr(args,'shared_text_input',False) or args.resident_shell_client
    if scrolling and (not portable_native_toolbar_build.selected(args) or (not args.paper_transitions and not args.resident_shell_client)):
        parser.error('--touch-scrolling requires --time-profile x4-native-time and --paper-transitions')
    cc = resolve_compiler()
    out = args.output_dir or ROOT/'dist/portable/wifi'
    flags=(['-DPORTABLE_SAFE_VIEWPORT'] if getattr(args,'safe_viewport',False) else [])+['-DPORTABLE_WIFI_SETTINGS_APP', '-DPORTABLE_WIFI_INSTANCE='+str(args.wifi_instance), '-DPORTABLE_WIFI_STORAGE_INSTANCE=6']
    if native_ui: flags += ['-DPORTABLE_WIFI_NATIVE_PRESENTATION','-DPORTABLE_NOVA_UI','-DPORTABLE_RF_NATIVE_CANVAS','-DPORTABLE_APP_OWNS_TOUCH_CHROME','-DPORTABLE_WATCH_NATIVE_HEIGHT','-DPORTABLE_WATCH_COLOR_ONLY','-DPORTABLE_WATCH_SAFE_INSETS']
    if args.resident_shell_client: flags.append('-DPORTABLE_WIFI_REMEMBER_CONNECTED')
    if atomic: flags+=['-DPORTABLE_WIFI_ATOMIC_EXPERIMENTAL','-I'+str(ROOT/'lib/RemoteFiles/vendor/tinydtls_sha2')]
    if availability: flags.append('-DPORTABLE_SETTINGS_AVAILABILITY')
    if policy: flags.append('-DPORTABLE_TEXT_INPUT_POLICY')
    if scrolling: flags.append('-DPORTABLE_TOUCH_SCROLL')
    if shared_text: flags+=['-DPORTABLE_TEXT_INPUT_CLIENT','-DPORTABLE_WIFI_PROFILES']
    flags.append('-DPORTABLE_DISPLAY_ROTATION='+str(args.display_rotation))
    if args.return_app:
        if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*\.elf', args.return_app):
            parser.error('Return app must be a plain .elf filename')
        flags.append('-DWIFI_RETURN_APP="'+args.return_app+'"')
    if args.wall_time: flags.append('-DPORTABLE_RTC_WALL_TIME')
    if args.nova_ui: flags.append('-DPORTABLE_NOVA_UI')
    if args.alarm_client: flags.append('-DPORTABLE_ALARM_CLIENT')
    if args.full_frames: flags.append('-DPORTABLE_FORCE_FULL_FRAMES')
    if args.navigation: flags.append('-DPORTABLE_INPUT_NAVIGATION')
    flags.append('-DPORTABLE_TOUCH_ROTATION='+str(args.touch_rotation))
    out.mkdir(parents=True, exist_ok=True)
    includes,native_flags,native_sources,native_receipt=portable_native_toolbar_build.configure(args,parser,ROOT,out,'wifi-settings');flags+=native_flags
    if native_receipt and args.paper_transitions:native_receipt['version']=portable_paper_build.CORE_MOTION_VERSIONS['wifi_settings']
    if scrolling:native_receipt['version']='1.1.11'
    if native_receipt:native_receipt['version']=portable_broadcast_build.version(args,'wifi_settings',native_receipt['version'])
    version=native_receipt['version'] if native_receipt else json.loads((ROOT/'Apps/wifi_settings.json').read_text())['version']
    flags+=portable_broadcast_build.flags(args)
    version=portable_quick_build.version(args,'wifi_settings',portable_idle_build.version(args,'wifi_settings',version))
    if policy: version='1.1.30'
    if native_ui: version='1.1.41'
    if time_sync:version='1.1.42'
    if native_receipt:native_receipt['version']=version
    flags.append('-DPORTABLE_WIFI_VERSION=\"'+version+'\"')
    quick_flags, quick_sources = portable_quick_build.configure(args,parser,ROOT,out,includes)
    flags += quick_flags
    includes,idle_flags,idle_sources=portable_foreground_idle_build.configure(args,parser,ROOT,out,includes)
    flags+=idle_flags
    quick_sources+=idle_sources
    time_sync_receipt=None
    if time_sync:
        key='sdk/driver/RiscTimeSampleV1.h'
        repo=args.foreground_idle_runtime_dir
        try:
            sample_pin=subprocess.check_output(['git','-C',str(repo),'rev-parse','HEAD'],text=True).strip()
            canonical_header=subprocess.check_output(['git','-C',str(repo),'show',sample_pin+':'+key])
        except subprocess.CalledProcessError as error:
            parser.error('Time sync requires committed canonical sampler header: '+str(error))
        if (ROOT/'lib/PortableApps/include/RiscTimeSampleV1.h').read_bytes()!=canonical_header:
            parser.error('Bundled sampler header differs from selected canonical Runtime')
        (includes/'RiscTimeSampleV1.h').write_bytes(canonical_header)
        flags+=['-DPORTABLE_WIFI_TIME_SYNC','-DPORTABLE_TIME_SYNC_SERVER='+json.dumps(server)]
        for name in TIME_SYNC_SOURCES:
            source=ROOT/'lib/PortableApps/src'/name
            if source not in quick_sources+native_sources:quick_sources.append(source)
        time_sync_receipt={'capability':'net.time-sample@1','server':server,'timeout_ms':5000,
            'sample_max_age_ms':1000,'transport':'unauthenticated SNTP','timezone_changed':False,
            'clock_commit':'shared AlarmClockEditV2 + native seed/readback',
            'header_source':key,'header_commit':sample_pin,
            'header_sha256':hashlib.sha256(canonical_header).hexdigest()}
        args.foreground_idle_receipt['header_origins']['RiscTimeSampleV1.h']={
            'source':key,'layer':'runtime-driver-addition','commit':sample_pin,
            'sha256':time_sync_receipt['header_sha256']}
        (out/'licenses/foreground-idle/SOURCES.json').write_text(json.dumps(args.foreground_idle_receipt,indent=2)+'\n')

    exports=portable_quick_build.exports(args,{'app_main', 'app_module_init', 'app_module_fini'})
    mapping = out/'wifi_settings.map'
    mapping.write_text('{ global: '+ '; '.join(sorted(exports))+'; local: *; };\n')
    catalog = out/'catalog.c'
    catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
    elf = out/'wifi_settings.elf'
    sources = [ROOT/'Apps/wifi_settings.c', ROOT/'lib/PortableApps/src/adapter.c', catalog]+quick_sources+native_sources
    if atomic: sources.append(ROOT/'lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c')
    compile_command=[cc, '-std=c11', '-Os', '-fPIC', '-mtext-section-literals', '-mlongcalls',
        '-fvisibility=hidden', '-ffreestanding', '-fno-builtin', '-nostdlib', '-nostartfiles', '-shared',
        '-Wl,--hash-style=sysv', '-Wl,--version-script='+str(mapping), '-Wall', '-Wextra', '-Werror',
        *flags, '-I'+str(includes),
        '-I'+str(ROOT/'lib/NativeApps/include'), *map(str, sources), '-o', str(elf)]
    subprocess.run(compile_command, check=True, timeout=120)
    symbols = subprocess.check_output([cc.removesuffix('gcc')+'nm', '-D', str(elf)], text=True)
    imports = {line.split()[-1] for line in symbols.splitlines() if ' U ' in ' '+line}
    allowed = {'risc_runtime_get_api','memcpy','memchr','memset','memcmp','strcmp','strlen','snprintf','strcpy','malloc','free'}
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
    if scrolling:native_receipt['version']='1.1.11'
    if native_receipt:native_receipt['version']=portable_broadcast_build.version(args,'wifi_settings',native_receipt['version'])
    version=native_receipt['version'] if native_receipt else json.loads((ROOT/'Apps/wifi_settings.json').read_text())['version']
    version=portable_quick_build.version(args,'wifi_settings',portable_idle_build.version(args,'wifi_settings',version))
    if policy: version='1.1.30'
    if native_ui: version='1.1.41'
    if time_sync:version='1.1.42'
    if native_receipt:native_receipt['version']=version
    manifest={'type':'application','id':'wifi_settings','version':version,'architecture':'xtensa-esp32s3',
        'file_name':'wifi_settings.elf','entry':'app_main','requires':[
            {'capability':'display.output','api':1}, {'capability':'input.touch.raw','api':1},
            {'capability':'net.wifi','api':1}]}
    if shared_text: manifest['requires'].append({'capability':'ui.text-input','api':1})
    if atomic: manifest['requires'].append({'capability':'storage.key-value.atomic','api':1})
    if availability: manifest['requires'].append({'capability':'storage.key-value.availability','api':1})
    if time_sync: manifest['requires'] += [{'capability':'net.time-sample','api':1},{'capability':'runtime.realtime-control','api':1}]
    if args.alarm_client: manifest['requires'].append({'capability':'alarm.service','api':1})
    manifest['requires'].append({'capability':'storage.key-value','api':1})
    if args.navigation: manifest['requires'].append({'capability':'input.navigation','api':1})
    portable_quick_build.requirements(args, manifest['requires'])
    manifest['requires'] = list({(item['capability'], item['api']): item for item in manifest['requires']}.values())
    portable_native_toolbar_build.requirements(args,manifest['requires'])
    portable_broadcast_build.requirements(args,manifest['requires'])
    portable_foreground_idle_build.requirements(args,manifest['requires'])
    (out/'wifi_settings.json').write_text(json.dumps(manifest,indent=2)+'\n')
    inputs=['Apps/wifi_settings.c','Apps/wifi_settings_portable.inc','Apps/wifi_settings.json','lib/NativeApps/include/T5AppApi.h', 'lib/NativeApps/include/T5UiApi.h', 'lib/NativeApps/include/T5StorageApi.h', 'lib/NativeApps/include/T5VideoApi.h', 'scripts/build_portable_wifi.py']
    # Include all shared-client inputs: the adapter can also carry the shared
    # Springboard presentation/fonts after the coordinated integration.
    inputs += [str(p.relative_to(ROOT)) for p in sorted((ROOT/'lib/PortableApps').rglob('*')) if p.is_file()]
    if (ROOT/'Apps/SpringboardPresentation.h').exists(): inputs.append('Apps/SpringboardPresentation.h')
    if time_sync:inputs.append('Apps/wifi_time_sync.inc')
    inputs += ['Apps/PaperPresentation.h', 'Apps/PaperFrame.h', 'scripts/portable_quick_build.py']
    if atomic:
        inputs+=['Apps/wifi_atomic.inc']
        inputs += [str(p.relative_to(ROOT)) for p in sorted((ROOT/'lib/RemoteFiles/vendor/tinydtls_sha2').glob('*')) if p.is_file()]
    if args.paper_transitions:inputs.append('scripts/portable_paper_build.py')
    inputs=sorted(set(inputs))
    for group in ['settings_fonts','fonts','paper_fonts']:
        source=ROOT/'lib/PortableApps'/group
        if source.exists():
            destination=out/'licenses'/group;destination.mkdir(parents=True,exist_ok=True)
            for notice in [*source.glob('LICENSE-*.txt'),*source.glob('SOURCES.json')]:
                shutil.copyfile(notice,destination/notice.name)
    destination=out/'licenses'/'portable-wifi';destination.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(ROOT/'LICENSE',destination/'System-Apps-LICENSE.txt')
    shutil.copyfile(ROOT/'lib/PortableApps/time/SOURCES.json',destination/'time-SOURCES.json')
    shutil.copyfile(ROOT/'lib/PortableApps/RTC_PROVENANCE.json',destination/'RTC-PROVENANCE.json')
    record={'purpose':'portable-development-artifact-not-deployment','version':version,
        'repository_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
        'working_tree_dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True).strip()),
        'compiler':subprocess.check_output([cc,'--version'],text=True).splitlines()[0],
        'sha256':hashlib.sha256(data).hexdigest(),'size_bytes':len(data),
        'imports':sorted(imports),'exports':sorted(exports),
        'compile_command':compile_command,
        'grant_bindings':{'net.wifi':args.wifi_instance,'storage.key-value':[6,1] if args.quick_actions else [6],
                          'other_providers':'selected by product integration owner; no grants applied'},
        'build_defines':flags,'wifi_instance':args.wifi_instance,'storage_instance':6,
        'home_app':args.home_app,'return_app':args.return_app,'display_rotation':args.display_rotation,
        'touch_scrolling':scrolling,'shared_text_input':shared_text,
        'quick_actions':args.quick_actions,'quick_radios':args.quick_radios,'wall_time':args.wall_time,
        'requested_capabilities':manifest['requires'],
        'full_frames':args.full_frames,'navigation':args.navigation,'touch_rotation':args.touch_rotation,
        'source_sha256':{p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in inputs}}
    if time_sync: record['time_server_sync']=time_sync_receipt
    if native_ui: record.update(watch_native_ui=True,safe_content_native_raster=True)
    record['shared_settings']={'busy_reload':'draft-safe-250ms','save_acknowledges':'native-journal','sd_checkpoint_confirmed':False,'hardware_qualified':False}
    if atomic:
        record['atomic_wifi']={'experimental':True,'capability':'storage.key-value.atomic@1','namespace':6,'copied_async':True,'native_map_and_intent_atomic':True,'sd_checkpoint_confirmed':False,'production_broker_connected':False}
        record['grant_bindings']['storage.key-value.atomic']=[6]
        record['grant_bindings']['storage.key-value']=[1]  # X4 radio policy remains read through KV1
        record['shared_settings']['save_acknowledges']='native-atomic-map-and-intent'
        notice=out/'licenses'/'wifi-intent-sha256';notice.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(ROOT/'lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c',notice/'sha2.c')
        shutil.copyfile(ROOT/'lib/RemoteFiles/vendor/tinydtls_sha2/sha2.h',notice/'sha2.h')
    portable_native_toolbar_build.record(args,record,manifest,native_receipt)
    if args.paper_transitions:record['paper_motion']=portable_paper_build.motion_receipt(ROOT)
    if native_receipt:
        for p in ['scripts/portable_native_toolbar_build.py','scripts/portable_alarm_build.py',native_receipt['profile_source']]:
            record['source_sha256'][p]=hashlib.sha256((ROOT/p).read_bytes()).hexdigest()
    portable_broadcast_build.record(args,ROOT,record,manifest,flags)
    portable_idle_build.record(args,record,flags)
    portable_quick_build.record(args,record)
    # Native toolbar bookkeeping defaults to legacy KV; this explicit atomic
    # Wi-Fi selection owns exact data/status namespaces, with no writer fallback.
    if atomic:
        record['grant_bindings']['storage.key-value']=[1]
        record['grant_bindings']['storage.key-value.atomic']=[6]
        if availability:
            record['grant_bindings']['storage.key-value.availability']=[6]
            record['settings_availability']={'experimental':True,'namespace':6,
                'capability':'storage.key-value.availability@1','default_enabled':False,
                'unavailable_edits':'temporary-ram','reload_boundary':'explicit-discard-or-clean-data-release',
                'pending_provider_grants':'clean-runtime-restart-may-be-required',
                'sd_checkpoint_confirmed':False,'hardware_qualified':False}
        record['required_grants']=[dict(item,instance_id=instance) for item in manifest['requires']
            for instance in record['grant_bindings'].get(item['capability'],[0])]
        record['grant_count']=len(record['required_grants'])
    portable_native_toolbar_build.write_admission(ROOT,out,manifest,record)
    portable_quick_build.record(args,record)
    portable_foreground_idle_build.record(args,record,flags,private_kv=() if atomic else (6,),compile_command=compile_command)
    (out/'wifi_settings-build-record.json').write_text(json.dumps(record,indent=2)+'\n')
    print('Portable Wi-Fi: target layout, ELF validator, import/export checks passed')

if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--time-server-sync',action='store_true',help='Sync UTC after owned Wi-Fi connection using checked shared alarm clock transaction')
    parser.add_argument('--time-server',default='pool.ntp.org',help='Explicit SNTP request server; only used with --time-server-sync')
    parser.add_argument('--watch-native-ui',action='store_true',help='Native safe-content Wi-Fi geometry and local keyboard; original font pixels')
    parser.add_argument('--safe-viewport',action='store_true',help='Use provider content bounds with full physical RGB565 raster')
    parser.add_argument('--shared-text-policy',action='store_true',help='X4 shared editor checkpoint progression; selected availability profile only')
    parser.add_argument('--experimental-settings-availability',action='store_true',help='Default-off exact namespace-6 availability; requires the experimental atomic X4 profile')
    parser.add_argument('--experimental-atomic-wifi',action='store_true',help='Disconnected X4 native atomic map+intent capability; never falls back to KV1')
    parser.add_argument('--shared-text-input',action='store_true',help='Use the separately loaded shared keyboard, required for resident X4 builds')
    parser.add_argument('--touch-scrolling',action='store_true',help='Selected paper lists with bounded touch momentum')
    parser.add_argument('--display-rotation',type=int,choices=[0,90],default=None)
    parser.add_argument('--return-app',help='App-owned root Back target; nested Back stays local')
    parser.add_argument('--wall-time',action='store_true',help='Explicit unchanged RTC wall-time policy')
    portable_broadcast_build.options(parser)
    portable_quick_build.options(parser)
    portable_foreground_idle_build.options(parser)
    portable_native_toolbar_build.options(parser)
    parser.add_argument('--nova-ui',action='store_true',help='Settings-derived 240x240 Nova utility profile')
    parser.add_argument('--alarm-client',action='store_true',help='Explicit alarm.service foreground overlay consumer')
    parser.add_argument('--full-frames',action='store_true',help='Disable optional partial-damage and previous-frame cache')
    parser.add_argument('--wifi-instance',type=int,default=0,help='Exact deployment-authorized net.wifi instance; 0 requires a unique provider')
    parser.add_argument('--navigation',action='store_true',help='Require a granted input.navigation@1 provider')
    parser.add_argument('--touch-rotation',type=int,choices=[0,180],default=0)
    parser.add_argument('--output-dir',type=Path)
    build(parser.parse_args(),parser)

#!/usr/bin/env python3
"""Build the shared Wi-Fi Settings app against the minimal capability client.

Development artifact only. The deployment supplies display.output@1,
input.touch.raw@1, net.wifi@1 and namespace-6 storage.key-value@1 grants; no additional runtime exports.
"""
import argparse
import hashlib
import json
import os
import re
import portable_broadcast_build
import portable_quick_build
import portable_idle_build
import portable_native_toolbar_build
import portable_paper_build
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def build(args, parser=None):
    parser = parser or argparse.ArgumentParser(description=__doc__)
    portable_native_toolbar_build.validate(args,parser)
    portable_broadcast_build.validate(args,parser,portable_native_toolbar_build.selected(args))
    scrolling=getattr(args,'touch_scrolling',False)
    shared_text=getattr(args,'shared_text_input',False) or args.resident_shell_client
    if scrolling and (not portable_native_toolbar_build.selected(args) or (not args.paper_transitions and not args.resident_shell_client)):
        parser.error('--touch-scrolling requires --time-profile x4-native-time and --paper-transitions')
    cc = os.environ.get('NATIVE_APP_CC') or shutil.which('xtensa-esp32s3-elf-gcc')
    if not cc:
        core = Path(os.environ.get('PLATFORMIO_CORE_DIR', Path.home()/'.platformio'))
        cc = str(core/'packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc')
    out = args.output_dir or ROOT/'dist/portable/wifi'
    flags=['-DPORTABLE_WIFI_SETTINGS_APP', '-DPORTABLE_WIFI_INSTANCE='+str(args.wifi_instance), '-DPORTABLE_WIFI_STORAGE_INSTANCE=6']
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
    if native_receipt:native_receipt['version']=version
    flags.append('-DPORTABLE_WIFI_VERSION=\"'+version+'\"')
    quick_flags, quick_sources = portable_quick_build.configure(args,parser,ROOT,out,includes)
    flags += quick_flags
    exports=portable_quick_build.exports(args,{'app_main', 'app_module_init', 'app_module_fini'})
    mapping = out/'wifi_settings.map'
    mapping.write_text('{ global: '+ '; '.join(sorted(exports))+'; local: *; };\n')
    catalog = out/'catalog.c'
    catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
    elf = out/'wifi_settings.elf'
    sources = [ROOT/'Apps/wifi_settings.c', ROOT/'lib/PortableApps/src/adapter.c', catalog]+quick_sources+native_sources
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
    if native_receipt:native_receipt['version']=version
    manifest={'type':'application','id':'wifi_settings','version':version,'architecture':'xtensa-esp32s3',
        'file_name':'wifi_settings.elf','entry':'app_main','requires':[
            {'capability':'display.output','api':1}, {'capability':'input.touch.raw','api':1},
            {'capability':'net.wifi','api':1}]}
    if shared_text: manifest['requires'].append({'capability':'ui.text-input','api':1})
    if args.alarm_client: manifest['requires'].append({'capability':'alarm.service','api':1})
    manifest['requires'].append({'capability':'storage.key-value','api':1})
    if args.navigation: manifest['requires'].append({'capability':'input.navigation','api':1})
    portable_quick_build.requirements(args, manifest['requires'])
    manifest['requires'] = list({(item['capability'], item['api']): item for item in manifest['requires']}.values())
    portable_native_toolbar_build.requirements(args,manifest['requires'])
    portable_broadcast_build.requirements(args,manifest['requires'])
    (out/'wifi_settings.json').write_text(json.dumps(manifest,indent=2)+'\n')
    inputs=['Apps/wifi_settings.c','Apps/wifi_settings_portable.inc','Apps/wifi_settings.json','lib/NativeApps/include/T5AppApi.h', 'lib/NativeApps/include/T5UiApi.h', 'lib/NativeApps/include/T5StorageApi.h', 'lib/NativeApps/include/T5VideoApi.h', 'scripts/build_portable_wifi.py']
    # Include all shared-client inputs: the adapter can also carry the shared
    # Springboard presentation/fonts after the coordinated integration.
    inputs += [str(p.relative_to(ROOT)) for p in sorted((ROOT/'lib/PortableApps').rglob('*')) if p.is_file()]
    if (ROOT/'Apps/SpringboardPresentation.h').exists(): inputs.append('Apps/SpringboardPresentation.h')
    inputs += ['Apps/PaperPresentation.h', 'Apps/PaperFrame.h', 'scripts/portable_quick_build.py']
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
    portable_native_toolbar_build.record(args,record,manifest,native_receipt)
    if args.paper_transitions:record['paper_motion']=portable_paper_build.motion_receipt(ROOT)
    if native_receipt:
        for p in ['scripts/portable_native_toolbar_build.py','scripts/portable_alarm_build.py',native_receipt['profile_source']]:
            record['source_sha256'][p]=hashlib.sha256((ROOT/p).read_bytes()).hexdigest()
    portable_broadcast_build.record(args,ROOT,record,manifest,flags)
    portable_idle_build.record(args,record,flags)
    portable_quick_build.record(args,record)
    portable_native_toolbar_build.write_admission(ROOT,out,manifest,record)
    portable_quick_build.record(args,record)
    (out/'wifi_settings-build-record.json').write_text(json.dumps(record,indent=2)+'\n')
    print('Portable Wi-Fi: target layout, ELF validator, import/export checks passed')

if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--shared-text-input',action='store_true',help='Use the separately loaded shared keyboard, required for resident X4 builds')
    parser.add_argument('--touch-scrolling',action='store_true',help='Selected paper lists with bounded touch momentum')
    parser.add_argument('--display-rotation',type=int,choices=[0,90],default=None)
    parser.add_argument('--return-app',help='App-owned root Back target; nested Back stays local')
    parser.add_argument('--wall-time',action='store_true',help='Explicit unchanged RTC wall-time policy')
    portable_broadcast_build.options(parser)
    portable_quick_build.options(parser)
    portable_native_toolbar_build.options(parser)
    parser.add_argument('--nova-ui',action='store_true',help='Settings-derived 240x240 Nova utility profile')
    parser.add_argument('--alarm-client',action='store_true',help='Explicit alarm.service foreground overlay consumer')
    parser.add_argument('--full-frames',action='store_true',help='Disable optional partial-damage and previous-frame cache')
    parser.add_argument('--wifi-instance',type=int,default=0,help='Exact deployment-authorized net.wifi instance; 0 requires a unique provider')
    parser.add_argument('--navigation',action='store_true',help='Require a granted input.navigation@1 provider')
    parser.add_argument('--touch-rotation',type=int,choices=[0,180],default=0)
    parser.add_argument('--output-dir',type=Path)
    build(parser.parse_args(),parser)

#!/usr/bin/env python3
"""Build the shared Settings app against the minimal capability client.

Development artifact only. The deployment supplies display.output@1,
input.touch.raw@1, rtc.clock@2 and namespace-1 storage.key-value@1 grants; no additional runtime exports.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def build(args):
    cc = os.environ.get('NATIVE_APP_CC') or shutil.which('xtensa-esp32s3-elf-gcc')
    if not cc:
        core = Path(os.environ.get('PLATFORMIO_CORE_DIR', Path.home()/'.platformio'))
        cc = str(core/'packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc')
    out = args.output_dir or ROOT/'dist/portable'
    flags=['-DPORTABLE_SETTINGS_APP']
    if args.sleep_settings: flags.append('-DPORTABLE_SLEEP_SETTINGS')
    if args.alarm_settings: flags.append('-DPORTABLE_ALARM_SETTINGS')
    if args.alarm_client: flags.append('-DPORTABLE_ALARM_CLIENT')
    if args.denver: flags.append('-DPORTABLE_RTC_UTC8_DENVER')
    if args.full_frames: flags.append('-DPORTABLE_FORCE_FULL_FRAMES')
    if args.navigation: flags.append('-DPORTABLE_INPUT_NAVIGATION')
    flags.append('-DPORTABLE_TOUCH_ROTATION='+str(args.touch_rotation))
    version=json.loads((ROOT/'Apps/settings.json').read_text())['version']
    flags.append('-DPORTABLE_SETTINGS_VERSION=\"'+version+'\"')
    out.mkdir(parents=True, exist_ok=True)
    exports = {'app_main', 'app_module_init', 'app_module_fini'}
    mapping = out/'settings.map'
    mapping.write_text('{ global: '+ '; '.join(sorted(exports))+'; local: *; };\n')
    catalog = out/'catalog.c'
    catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
    elf = out/'settings.elf'
    sources = [ROOT/'Apps/settings.c', ROOT/'lib/PortableApps/src/adapter.c', catalog]
    subprocess.run([cc, '-std=c11', '-Os', '-fPIC', '-mtext-section-literals', '-mlongcalls',
        '-fvisibility=hidden', '-ffreestanding', '-fno-builtin', '-nostdlib', '-nostartfiles', '-shared',
        '-Wl,--hash-style=sysv', '-Wl,--version-script='+str(mapping), '-Wall', '-Wextra', '-Werror',
        *flags, '-I'+str(ROOT/'lib/PortableApps/include'),
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
    version=json.loads((ROOT/'Apps/settings.json').read_text())['version']
    manifest={'type':'application','id':'settings','version':version,'architecture':'xtensa-esp32s3',
        'file_name':'settings.elf','entry':'app_main','requires':[
            {'capability':'display.output','api':1}, {'capability':'input.touch.raw','api':1},
            {'capability':'rtc.clock','api':2}]}
    if args.alarm_client: manifest['requires'].append({'capability':'alarm.service','api':1})
    manifest['requires'].append({'capability':'storage.key-value','api':1})
    if args.navigation: manifest['requires'].append({'capability':'input.navigation','api':1})
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
    inputs=sorted(set(inputs))
    for group in ['settings_fonts','fonts']:
        source=ROOT/'lib/PortableApps'/group
        if source.exists():
            destination=out/'licenses'/group;destination.mkdir(parents=True,exist_ok=True)
            for notice in [*source.glob('LICENSE-*.txt'),*source.glob('SOURCES.json')]:
                shutil.copyfile(notice,destination/notice.name)
    destination=out/'licenses'/'portable-settings';destination.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(ROOT/'LICENSE',destination/'System-Apps-LICENSE.txt')
    shutil.copyfile(ROOT/'lib/PortableApps/time/SOURCES.json',destination/'time-SOURCES.json')
    shutil.copyfile(ROOT/'lib/PortableApps/RTC_PROVENANCE.json',destination/'RTC-PROVENANCE.json')
    record={'purpose':'portable-development-artifact-not-deployment','version':version,
        'repository_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
        'working_tree_dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True).strip()),
        'compiler':subprocess.check_output([cc,'--version'],text=True).splitlines()[0],
        'sha256':hashlib.sha256(data).hexdigest(),'size_bytes':len(data),
        'imports':sorted(imports),'exports':sorted(exports),
        'build_defines':flags,'time_policy':'rtc-utc8-to-America-Denver' if args.denver else 'identity-raw',
        'full_frames':args.full_frames,'navigation':args.navigation,'touch_rotation':args.touch_rotation,
        'source_sha256':{p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in inputs}}
    (out/'settings-build-record.json').write_text(json.dumps(record,indent=2)+'\n')
    print('Portable Settings: target layout, ELF validator, import/export checks passed')

if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--alarm-client',action='store_true',help='Explicit alarm.service foreground overlay consumer')
    parser.add_argument('--alarm-settings',action='store_true',help='Explicit namespace-1 alert mode choice')
    parser.add_argument('--sleep-settings',action='store_true',help='Enable explicit namespace-1 sleep choice; requires storage.key-value@1 grant')
    parser.add_argument('--denver',action='store_true',help='Explicit deployment policy: RTC fixed UTC+08, display America/Denver')
    parser.add_argument('--full-frames',action='store_true',help='Disable optional partial-damage and previous-frame cache')
    parser.add_argument('--navigation',action='store_true',help='Require a granted input.navigation@1 provider')
    parser.add_argument('--touch-rotation',type=int,choices=[0,180],default=0)
    parser.add_argument('--output-dir',type=Path)
    build(parser.parse_args())

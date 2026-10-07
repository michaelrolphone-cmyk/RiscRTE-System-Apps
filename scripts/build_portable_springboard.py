#!/usr/bin/env python3
"""Build the shared Springboard app against the minimal capability client.

Development artifact only. The deployment supplies display.output@1,
input.touch.raw@1 grants; RTC requires an explicitly selected deployment policy; no additional runtime exports.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def build():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--nova-ui",action="store_true",help="Settings-derived 240x240 shared utility profile")
    parser.add_argument("--alarm-client",action="store_true",help="Explicit alarm.service foreground overlay consumer")
    parser.add_argument("--denver",action="store_true",help="Select RTC UTC+08 to America/Denver display policy")
    parser.add_argument("--rotation","--touch-rotation",dest="rotation",type=int,choices=[0,180],default=0)
    parser.add_argument("--output-dir",type=Path,default=ROOT/"dist/portable")
    parser.add_argument("--retained-rgb565-handoff",action="store_true",help="Opt in only when the deployment guarantees a completed retained frame; see PORTABLE_TRANSITIONS.md")
    parser.add_argument("--full-frames",action="store_true",help="Disable optional partial-damage and previous-frame cache")
    parser.add_argument("--handoff-ms", type=int, choices=[60,180], default=180)
    parser.add_argument("--return-app", help="Explicit root-Back destination .elf")
    parser.add_argument("--catalog-limit",type=int,choices=[17,18,19,20],default=17,
                        help="18 is opt-in for the SDR Springboard; historical default is 17")
    args=parser.parse_args()
    flags=["-DPORTABLE_TOUCH_ROTATION="+str(args.rotation)]+(["-DPORTABLE_RTC_UTC8_DENVER"] if args.denver else [])
    if args.catalog_limit != 17: flags.append("-DPORTABLE_CATALOG_LIMIT="+str(args.catalog_limit))
    if args.handoff_ms!=180: flags.append("-DPORTABLE_HANDOFF_EAGER_MS="+str(args.handoff_ms))
    if args.return_app: flags.append('-DPORTABLE_RETURN_APP="'+args.return_app+'"')
    if args.nova_ui: flags.append("-DPORTABLE_NOVA_UI")
    if args.alarm_client: flags.append("-DPORTABLE_ALARM_CLIENT")
    if args.full_frames: flags.append("-DPORTABLE_FORCE_FULL_FRAMES")
    if args.retained_rgb565_handoff: flags.append("-DPORTABLE_RETAINED_RGB565_HANDOFF")
    cc = os.environ.get('NATIVE_APP_CC') or shutil.which('xtensa-esp32s3-elf-gcc')
    if not cc:
        core = Path(os.environ.get('PLATFORMIO_CORE_DIR', Path.home()/'.platformio'))
        cc = str(core/'packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc')
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    notices=out/'licenses/portable-apps';notices.mkdir(parents=True,exist_ok=True)
    for name in ['LICENSE-FontAwesome.txt','LICENSE-Orbitron.txt','LICENSE-Rajdhani.txt','SOURCES.json']:
        shutil.copyfile(ROOT/'lib/PortableApps/fonts'/name,notices/name)
    exports = {'app_main', 'app_module_init', 'app_module_fini'}
    mapping = out/'springboard.map'
    mapping.write_text('{ global: '+ '; '.join(sorted(exports))+'; local: *; };\n')
    catalog = out/'springboard-catalog.c'
    catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
    elf = out/'springboard.elf'
    sources = [ROOT/'Apps/springboard.c', ROOT/'lib/PortableApps/src/adapter.c', ROOT/'lib/NativeApps/src/SingleFloatDivisionCompat.c', catalog]
    subprocess.run([cc, '-std=c11', '-Os', '-fPIC', '-mtext-section-literals', '-mlongcalls',
        '-fvisibility=hidden', '-ffreestanding', '-fno-builtin', '-nostdlib', '-nostartfiles', '-shared',
        '-Wl,--hash-style=sysv', '-Wl,--version-script='+str(mapping), '-Wall', '-Wextra', '-Werror', *flags,
        '-I'+str(ROOT/'lib/PortableApps/include'),
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
    version=json.loads((ROOT/'Apps/springboard.json').read_text())['version']
    manifest={'type':'application','id':'springboard','version':version,'architecture':'xtensa-esp32s3',
        'file_name':'springboard.elf','entry':'app_main','requires':[
            {'capability':'display.output','api':1}, {'capability':'input.touch.raw','api':1},
]}
    if args.alarm_client: manifest["requires"].append({"capability":"alarm.service","api":1})
    if args.denver: manifest['requires'].append({'capability':'rtc.clock','api':2})
    (out/'springboard.json').write_text(json.dumps(manifest,indent=2)+'\n')
    inputs=['lib/PortableApps/include/PortableTransition.h','Apps/springboard.c','Apps/springboard.json','lib/PortableApps/src/adapter.c',
            'lib/PortableApps/src/nova.inc','Apps/springboard_nova.inc','Apps/springboard_motion.h','Apps/SpringboardPresentation.h','lib/PortableApps/fonts/icons.inc','lib/PortableApps/fonts/text.inc','lib/PortableApps/fonts/SOURCES.json','lib/NativeApps/src/SingleFloatDivisionCompat.c','lib/PortableApps/include/PortableRtcClock.h',
            'lib/PortableApps/RTC_PROVENANCE.json','lib/PortableApps/SOURCES.json']
    inputs += ['lib/PortableApps/include/'+name for name in json.loads((ROOT/'lib/PortableApps/SOURCES.json').read_text())]
    inputs += ['lib/PortableApps/include/PortableTime.h','lib/PortableApps/time/denver/display_time.h','lib/PortableApps/time/denver/twatch_calendar.h','lib/PortableApps/time/denver/twatch_caps.h','lib/PortableApps/time/SOURCES.json']
    inputs += ['lib/PortableApps/include/PortableApps.h','lib/PortableApps/include/PortableTouch.h',
               'lib/NativeApps/include/T5AppApi.h','lib/NativeApps/include/T5UiApi.h',
               'lib/NativeApps/include/T5StorageApi.h','lib/NativeApps/include/T5VideoApi.h']
    inputs += [str(p.relative_to(ROOT)) for p in sorted((ROOT/'lib/PortableApps').rglob('*')) if p.is_file()]
    inputs=sorted(set(inputs))
    record={'purpose':'portable-development-artifact-not-deployment','version':version,
        'repository_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
        'catalog_limit':args.catalog_limit,'full_frames':args.full_frames,'retained_rgb565_handoff':args.retained_rgb565_handoff,'touch_rotation':args.rotation,'clock_policy':'rtc-utc8-america-denver' if args.denver else 'unavailable',
        'working_tree_dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True).strip()),
        'compiler':subprocess.check_output([cc,'--version'],text=True).splitlines()[0],
        'sha256':hashlib.sha256(data).hexdigest(),'size_bytes':len(data),
        'imports':sorted(imports),'exports':sorted(exports),
        'source_sha256':{p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in inputs}}
    (out/'springboard-build-record.json').write_text(json.dumps(record,indent=2)+'\n')
    print('Portable Springboard: target layout, ELF validator, import/export checks passed')

if __name__ == '__main__': build()

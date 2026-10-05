#!/usr/bin/env python3
"""Build the capability-only NOVA-7 File Browser. No firmware filesystem/UI ABI."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def build(args):
    cc=os.environ.get('NATIVE_APP_CC') or shutil.which('xtensa-esp32s3-elf-gcc')
    if not cc:
        cc=str(Path(os.environ.get('PLATFORMIO_CORE_DIR',Path.home()/'.platformio'))/'packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc')
    out=args.output_dir or ROOT/'dist/portable/file-browser';out.mkdir(parents=True,exist_ok=True)
    flags=['-DPORTABLE_FILE_BROWSER_APP','-DPORTABLE_NOVA_UI','-DPORTABLE_APP_OWNS_TOUCH_CHROME','-DPORTABLE_FORCE_FULL_FRAMES',f'-DPORTABLE_TOUCH_ROTATION={args.touch_rotation}']
    if args.alarm_client:flags.append('-DPORTABLE_ALARM_CLIENT')
    if args.navigation:flags.append('-DPORTABLE_INPUT_NAVIGATION')
    if args.quick_controls:
        if not args.alarm_client:raise ValueError('Quick controls require the retained alarm-client surface')
        flags+=['-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_QUICK_RADIOS','-DPORTABLE_RTC_UTC8_DENVER']
    exports={'app_main','app_module_init','app_module_fini'}
    mapping=out/'file_browser.map';mapping.write_text('{ global: '+'; '.join(sorted(exports))+'; local: *; };\n')
    catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
    sources=[ROOT/'Apps/file_browser.c',ROOT/'lib/PortableApps/src/adapter.c',catalog]
    if args.quick_controls:sources += [ROOT/'lib/PortableApps/src'/n for n in ('quick_actions.c','quick_render.c','quick_session.c','quick_radios.c')]
    elf=out/'file_browser.elf'
    subprocess.run([cc,'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wl,--version-script='+str(mapping),'-Wall','-Wextra','-Werror',*flags,'-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),*map(str,sources),'-lgcc','-o',str(elf)],check=True)
    symbols=subprocess.check_output([cc.removesuffix('gcc')+'nm','-D',str(elf)],text=True)
    imports={x.split()[-1] for x in symbols.splitlines() if ' U ' in ' '+x}
    actual={x.split()[-1] for x in symbols.splitlines() if len(x.split())>=3 and x.split()[-2] in ('T','D','B','R')}
    allowed={'risc_runtime_get_api','memcpy','memset','memcmp','strcmp','strlen','strncmp','strrchr','snprintf','malloc','free','strcpy','memchr'}
    if not imports<=allowed or actual!=exports:raise ValueError(f'Unexpected ELF ABI: {imports-allowed}, {actual}')
    data=elf.read_bytes()
    if data[:7]!=b'\x7fELF\x01\x01\x01' or data[16:20]!=b'\x03\x00\x5e\x00':raise ValueError('Expected Xtensa ELF32 ET_DYN')
    validator=out/'validate-elf'
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'),str(ROOT/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
    subprocess.run([str(validator),str(elf)],check=True)
    manifest=json.loads((ROOT/'Apps/native/file_browser.json').read_text())
    if args.alarm_client:manifest['requires'].append({'capability':'alarm.service','api':1})
    if args.navigation:manifest['requires'].append({'capability':'input.navigation','api':1})
    if args.quick_controls:manifest['requires'] += [{'capability':c,'api':v} for c,v in [('storage.key-value',1),('rtc.clock',2),('net.wifi',1),('bluetooth.hci',1)]]
    (out/'file_browser.json').write_text(json.dumps(manifest,indent=2)+'\n')
    files=[ROOT/'Apps/file_browser.c',ROOT/'Apps/file_browser_portable.inc',ROOT/'Apps/native/file_browser.json',ROOT/'lib/NativeApps/include/FileBrowserModel.h',Path(__file__)]
    files += [p for p in (ROOT/'lib/PortableApps').rglob('*') if p.is_file()]
    record={'schema':1,'version':manifest['version'],'repository_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'working_tree_dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True)),'compiler':subprocess.check_output([cc,'--version'],text=True).splitlines()[0],'defines':flags,'imports':sorted(imports),'exports':sorted(exports),'sha256':hashlib.sha256(data).hexdigest(),'size_bytes':len(data),'source_sha256':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(set(files))}}
    (out/'file_browser-build-record.json').write_text(json.dumps(record,indent=2)+'\n')
    licenses=out/'licenses';licenses.mkdir(exist_ok=True)
    shutil.copyfile(ROOT/'LICENSE',licenses/'System-Apps-LICENSE.txt')
    for p in (ROOT/'lib/PortableApps/settings_fonts').glob('LICENSE-*'):shutil.copyfile(p,licenses/p.name)
    print('Portable File Browser: Xtensa ELF, real loader validation and exact imports/exports passed')
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output-dir',type=Path);p.add_argument('--touch-rotation',type=int,choices=[0,180],default=0);p.add_argument('--alarm-client',action='store_true');p.add_argument('--navigation',action='store_true');p.add_argument('--quick-controls',action='store_true');build(p.parse_args())

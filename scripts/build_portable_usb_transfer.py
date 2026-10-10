#!/usr/bin/env python3
"""Build the dedicated USB-device SD transfer app without a whole app cohort."""
import argparse, hashlib, json, os, shutil, subprocess
import portable_quick_build
import portable_alarm_build
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
FLAGS=['-DPORTABLE_USB_TRANSFER_APP','-DPORTABLE_APP_LAUNCH_GUARD',
       '-DPORTABLE_APP_OWNS_TOUCH_CHROME','-DPORTABLE_INPUT_NAVIGATION',
       '-DPORTABLE_DISPLAY_ROTATION=90','-DPORTABLE_HOME_APP="default.elf"',
       '-DPORTABLE_RETURN_APP="springboard.elf"']

def build(args):
    sdk=args.msc_sdk.resolve();header=sdk/'RiscUsbDeviceMscV1.h'
    if not header.is_file():raise ValueError('Canonical USB MSC SDK header is required')
    out=args.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
    includes=out/'include';shutil.copytree(ROOT/'lib/PortableApps/include',includes,dirs_exist_ok=True)
    shutil.copyfile(header,includes/header.name)
    cc=os.environ.get('NATIVE_APP_CC') or shutil.which('xtensa-esp32s3-elf-gcc')
    if not cc:cc=str(Path(os.environ.get('PLATFORMIO_CORE_DIR',Path.home()/'.platformio'))/'packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc')
    flags=list(FLAGS)
    if args.resident_shell_host:raise ValueError('USB transfer is a foreground client, never a resident host')
    args.alarm_client=args.resident_shell_client
    if args.resident_shell_client:
        if not args.tagged_alarm_utilities:raise ValueError('Resident USB requires the canonical tagged alarm SDK')
        portable_alarm_build.stage(args,argparse.ArgumentParser(),out,includes)
        selected,sources=portable_quick_build.configure(args,argparse.ArgumentParser(),ROOT,out,includes)
        assert not sources
        flags+=selected+['-DPORTABLE_NATIVE_CUSTODY_FENCE','-DPORTABLE_ALARM_TERMINAL_RETENTION','-DPORTABLE_ALARM_CLIENT','-DALARM_SERVICE_TAGGED_V2']
    exports=portable_quick_build.exports(args,{'app_main','app_module_init','app_module_fini'})
    mapping=out/'usb_sd_transfer.map';mapping.write_text('{ global: '+'; '.join(sorted(exports))+'; local: *; };\n')
    catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
    elf=out/'usb_sd_transfer.elf';sources=[ROOT/'Apps/usb_sd_transfer.c',ROOT/'lib/PortableApps/src/adapter.c',catalog]
    subprocess.run([cc,'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wl,--version-script='+str(mapping),'-Wall','-Wextra','-Werror',*flags,'-I'+str(includes),'-I'+str(ROOT/'lib/NativeApps/include'),*map(str,sources),'-lgcc','-o',str(elf)],check=True)
    symbols=subprocess.check_output([cc.removesuffix('gcc')+'nm','-D',str(elf)],text=True)
    imports={x.split()[-1] for x in symbols.splitlines() if ' U ' in ' '+x}
    actual={x.split()[-1] for x in symbols.splitlines() if len(x.split())>=3 and x.split()[-2] in ('T','D','B','R')}
    allowed={'risc_runtime_get_api','memcpy','memset','memcmp','strcmp','strlen','snprintf','malloc','free','strcpy'}
    if not imports<=allowed or actual!=exports:raise ValueError(f'Unexpected ELF ABI: {imports-allowed}, {actual}')
    data=elf.read_bytes()
    if data[:7]!=b'\x7fELF\x01\x01\x01' or data[16:20]!=b'\x03\x00\x5e\x00':raise ValueError('Expected Xtensa ELF32 ET_DYN')
    validator=out/'validate-elf'
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'),str(ROOT/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
    subprocess.run([str(validator),str(elf)],check=True)
    manifest=json.loads((ROOT/'Apps/native/usb_sd_transfer.json').read_text())
    manifest['version']=portable_quick_build.version(args,'usb_sd_transfer',manifest['version'])
    if args.resident_shell_client:
        manifest['requires'].append({'capability':'alarm.service','api':2})
    (out/'usb_sd_transfer.json').write_text(json.dumps(manifest,indent=2)+'\n')
    files=[ROOT/'Apps/usb_sd_transfer.c',ROOT/'Apps/native/usb_sd_transfer.json',ROOT/'Apps/PaperPresentation.h',Path(__file__)]
    files += [p for p in (ROOT/'lib/PortableApps').rglob('*') if p.is_file()]
    sha=lambda b:hashlib.sha256(b).hexdigest()
    record={'schema':1,'version':manifest['version'],'repository_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'working_tree_dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True)),'compiler':subprocess.check_output([cc,'--version'],text=True).splitlines()[0],'defines':flags,'imports':sorted(imports),'exports':sorted(exports),'sha256':sha(data),'size_bytes':len(data),'source_sha256':{str(p.relative_to(ROOT)):sha(p.read_bytes()) for p in sorted(set(files))},'sdk_sha256':{header.name:sha(header.read_bytes())},'capability':{'name':'usb.device.msc','api':1,'instance_id':0},'poll_interval_ms':2,'connected_poll_interval_ms':1,'usb_role':'device','sleep':False,'navigation_requires_release':True,'configured_stop_requires_cable_confirmation':True,'sd_preparation':{'extension_tag':'0x554d5031','extension_version':1,'explicit_step':True,'settled_screen_required':True,'max_steps_per_input_poll':1,'wait_poll_prepares':False}}
    portable_quick_build.record(args,record)
    (out/'usb_sd_transfer-build-record.json').write_text(json.dumps(record,indent=2)+'\n')
    licenses=out/'licenses';licenses.mkdir(exist_ok=True);shutil.copyfile(ROOT/'LICENSE',licenses/'System-Apps-LICENSE.txt')
    for group in ('settings_fonts','fonts','paper_fonts'):
        target=licenses/group;target.mkdir(exist_ok=True)
        for p in (ROOT/'lib/PortableApps'/group).glob('LICENSE*'):shutil.copyfile(p,target/p.name)
        source=ROOT/'lib/PortableApps'/group/'SOURCES.json'
        if source.exists():shutil.copyfile(source,target/source.name)
    print('USB SD Transfer: Xtensa ELF, loader validation and exact imports/exports passed')
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);portable_quick_build.options(p);portable_alarm_build.options(p);p.add_argument('--msc-sdk',type=Path,required=True);p.add_argument('--output-dir',type=Path,default=ROOT/'dist/portable/usb-transfer');build(p.parse_args())

#!/usr/bin/env python3
"""Build the capability-only NOVA-7 File Browser. No firmware filesystem/UI ABI."""
import argparse,hashlib,json,os,re,shutil,subprocess
from pathlib import Path
import portable_broadcast_build
import portable_quick_build
import portable_idle_build
import portable_native_toolbar_build
import portable_paper_build
ROOT=Path(__file__).resolve().parents[1]
def build(args,parser=None):
    parser=parser or argparse.ArgumentParser(description=__doc__)
    portable_native_toolbar_build.validate(args,parser)
    portable_broadcast_build.validate(args,parser,portable_native_toolbar_build.selected(args))
    scrolling=getattr(args,'touch_scrolling',False)
    if scrolling and (not portable_native_toolbar_build.selected(args) or (not args.paper_transitions and not args.resident_shell_client)):
        parser.error('--touch-scrolling requires --time-profile x4-native-time and --paper-transitions')
    if not re.fullmatch(r'[a-z][a-z0-9.-]*',args.storage_capability):raise ValueError('Invalid storage capability')
    if not re.fullmatch(r'[A-Za-z0-9_.-]+(?:/[A-Za-z0-9_.-]+)*\.elf',args.return_app) or any(part in ('.','..') for part in args.return_app.split('/')):raise ValueError('Return app must be a normalized installed relative ELF path')
    if not 0<=args.storage_instance<=0x7fffffff:raise ValueError('Storage instance is out of range')
    if args.storage_capability=='storage.installed-files' and args.storage_instance:
        parser.error('storage.installed-files is a Runtime service at instance 0; SD requires --storage-capability storage.volume')
    if args.secondary_storage_instance is not None:
        if not 0<args.secondary_storage_instance<=0x7fffffff:raise ValueError('Secondary storage requires an explicit valid instance')
        if args.storage_capability=='storage.volume' and (not args.storage_instance or args.storage_instance==args.secondary_storage_instance):raise ValueError('Two storage.volume providers require distinct explicit instances')
    cc=os.environ.get('NATIVE_APP_CC') or shutil.which('xtensa-esp32s3-elf-gcc')
    if not cc:
        cc=str(Path(os.environ.get('PLATFORMIO_CORE_DIR',Path.home()/'.platformio'))/'packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc')
    out=args.output_dir or ROOT/'dist/portable/file-browser';out.mkdir(parents=True,exist_ok=True)
    flags=['-DPORTABLE_FILE_BROWSER_APP','-DPORTABLE_NOVA_UI','-DPORTABLE_APP_OWNS_TOUCH_CHROME','-DPORTABLE_FORCE_FULL_FRAMES',f'-DPORTABLE_TOUCH_ROTATION={args.touch_rotation}',f'-DPORTABLE_DISPLAY_ROTATION={args.display_rotation}',f'-DPORTABLE_FILE_BROWSER_CAPABILITY=\"{args.storage_capability}\"',f'-DPORTABLE_FILE_BROWSER_INSTANCE={args.storage_instance}u',f'-DFILE_BROWSER_RETURN_APP=\"{args.return_app}\"']
    if scrolling:flags.append('-DPORTABLE_TOUCH_SCROLL')
    if args.secondary_storage_instance is not None:flags.append(f'-DPORTABLE_FILE_BROWSER_SECONDARY_INSTANCE={args.secondary_storage_instance}u')
    if args.file_handlers:flags.append('-DPORTABLE_FILE_BROWSER_HANDLERS')
    if args.alarm_client:flags.append('-DPORTABLE_ALARM_CLIENT')
    if args.navigation:flags.append('-DPORTABLE_INPUT_NAVIGATION')
    if args.quick_controls:
        if args.wall_time:parser.error('Legacy Watch --quick-controls uses its explicit Denver policy')
        args.quick_actions=args.quick_radios=True
        flags+=['-DPORTABLE_RTC_UTC8_DENVER']
    if args.wall_time:flags+=['-DPORTABLE_RTC_WALL_TIME']
    includes,native_flags,native_sources,native_receipt=portable_native_toolbar_build.configure(args,parser,ROOT,out,'file-browser');flags+=native_flags
    # All portable profiles share this controller fix; stage logging does not
    # allocate another version. Explicit future motion profiles may override it.
    if native_receipt:native_receipt['version']=json.loads((ROOT/'Apps/native/file_browser.json').read_text())['version']
    if native_receipt and args.paper_transitions:native_receipt['version']=portable_paper_build.CORE_MOTION_VERSIONS['file_browser']
    if scrolling:native_receipt['version']='1.5.9'
    if native_receipt:native_receipt['version']=portable_broadcast_build.version(args,'file_browser',native_receipt['version'])
    flags+=portable_broadcast_build.flags(args)
    # Native X4 paper must retain damage history, including Quick over Files.
    if native_receipt:flags.remove('-DPORTABLE_FORCE_FULL_FRAMES')
    quick_flags,quick_sources=portable_quick_build.configure(args,parser,ROOT,out,includes);flags+=quick_flags
    exports=portable_quick_build.exports(args,{'app_main','app_module_init','app_module_fini'})
    mapping=out/'file_browser.map';mapping.write_text('{ global: '+'; '.join(sorted(exports))+'; local: *; };\n')
    catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
    sources=[ROOT/'Apps/file_browser.c',ROOT/'lib/PortableApps/src/adapter.c',catalog]
    sources+=quick_sources+native_sources
    elf=out/'file_browser.elf'
    subprocess.run([cc,'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wl,--version-script='+str(mapping),'-Wall','-Wextra','-Werror',*flags,'-I'+str(includes),'-I'+str(ROOT/'lib/NativeApps/include'),*map(str,sources),'-lgcc','-o',str(elf)],check=True)
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
    for requirement in manifest['requires']:
        if requirement['capability']=='storage.installed-files':requirement['capability']=args.storage_capability
    if args.secondary_storage_instance is not None and args.storage_capability!='storage.volume':manifest['requires'].append({'capability':'storage.volume','api':1})
    if args.file_handlers:manifest['requires'].append({'capability':'file.open','api':1})
    if args.alarm_client:manifest['requires'].append({'capability':'alarm.service','api':1})
    if args.navigation:manifest['requires'].append({'capability':'input.navigation','api':1})
    portable_quick_build.requirements(args,manifest['requires'])
    portable_native_toolbar_build.requirements(args,manifest['requires'])
    portable_broadcast_build.requirements(args,manifest['requires'])
    if native_receipt:manifest['version']=native_receipt['version']
    manifest['version']=portable_quick_build.version(args,'file_browser',portable_idle_build.version(args,'file_browser',manifest['version']))
    if native_receipt:native_receipt['version']=manifest['version']
    (out/'file_browser.json').write_text(json.dumps(manifest,indent=2)+'\n')
    files=[ROOT/'Apps/file_browser.c',ROOT/'Apps/file_browser_portable.inc',ROOT/'Apps/file_browser_paper.inc',ROOT/'Apps/file_browser_operations.inc',ROOT/'Apps/PaperPresentation.h',ROOT/'Apps/PaperFrame.h',ROOT/'lib/NativeApps/include/T5FileOpenApi.h',ROOT/'Apps/native/file_browser.json',ROOT/'lib/NativeApps/include/FileBrowserModel.h',Path(__file__)]
    if scrolling:files += [ROOT/'Apps/file_browser_scroll_model.inc',ROOT/'Apps/file_browser_scroll_view.inc']
    files += [p for p in (ROOT/'lib/PortableApps').rglob('*') if p.is_file()]
    if args.paper_transitions:files.append(ROOT/'scripts/portable_paper_build.py')
    record={'schema':1,'version':manifest['version'],'repository_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'working_tree_dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True)),'compiler':subprocess.check_output([cc,'--version'],text=True).splitlines()[0],'defines':flags,'imports':sorted(imports),'exports':sorted(exports),'sha256':hashlib.sha256(data).hexdigest(),'size_bytes':len(data),'source_sha256':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(set(files))}}
    record['touch_scrolling']=scrolling
    portable_native_toolbar_build.record(args,record,manifest,native_receipt)
    record['storage_selection']={'primary':{'capability':args.storage_capability,'api':1,'instance_id':args.storage_instance},
                                 'secondary':None if args.secondary_storage_instance is None else {'capability':'storage.volume','api':1,'instance_id':args.secondary_storage_instance}}
    if args.paper_transitions:record['paper_motion']=portable_paper_build.motion_receipt(ROOT)
    if native_receipt:
        for p in ['scripts/portable_native_toolbar_build.py','scripts/portable_alarm_build.py',native_receipt['profile_source']]:
            record['source_sha256'][p]=hashlib.sha256((ROOT/p).read_bytes()).hexdigest()
    portable_broadcast_build.record(args,ROOT,record,manifest,flags)
    portable_idle_build.record(args,record,flags)
    portable_quick_build.record(args,record)
    portable_native_toolbar_build.write_admission(ROOT,out,manifest,record)
    portable_quick_build.record(args,record)
    (out/'file_browser-build-record.json').write_text(json.dumps(record,indent=2)+'\n')
    licenses=out/'licenses';licenses.mkdir(exist_ok=True)
    shutil.copyfile(ROOT/'LICENSE',licenses/'System-Apps-LICENSE.txt')
    for group in ('settings_fonts','fonts','paper_fonts'):
        destination=licenses/group;destination.mkdir(exist_ok=True)
        for p in (ROOT/'lib/PortableApps'/group).glob('LICENSE*'):shutil.copyfile(p,destination/p.name)
        source=ROOT/'lib/PortableApps'/group/'SOURCES.json'
        if source.exists():shutil.copyfile(source,destination/source.name)
    print('Portable File Browser: Xtensa ELF, real loader validation and exact imports/exports passed')
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--touch-scrolling',action='store_true',help='Selected paper lists with bounded touch momentum');p.add_argument('--output-dir',type=Path);p.add_argument('--display-rotation',type=int,choices=[0,90,180,270],default=None);p.add_argument('--storage-capability',default='storage.installed-files');p.add_argument('--storage-instance',type=int,default=0);p.add_argument('--secondary-storage-instance',type=int);p.add_argument('--return-app',default='springboard.elf');p.add_argument('--file-handlers',action='store_true');p.add_argument('--touch-rotation',type=int,choices=[0,180],default=0);p.add_argument('--alarm-client',action='store_true');p.add_argument('--navigation',action='store_true');p.add_argument('--quick-controls',action='store_true',help='Legacy Watch radio+Denver profile');p.add_argument('--wall-time',action='store_true');portable_quick_build.options(p);portable_broadcast_build.options(p);portable_native_toolbar_build.options(p);build(p.parse_args(),p)

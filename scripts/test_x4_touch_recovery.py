#!/usr/bin/env python3
"""Selected X4 GT911, shared consumer, and actual HID app/adapter recovery qualification."""
import argparse,hashlib,json,os,shutil,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(cmd,**kw):return subprocess.run(list(map(str,cmd)),check=True,**kw)
def replace(text,old,new):
    assert text.count(old)==1,(old,text.count(old))
    return text.replace(old,new)
p=argparse.ArgumentParser(description=__doc__)
for name in ('product','utilities','runtime','sdk'):p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--output-dir',type=Path,default=ROOT/'build/x4-touch-recovery')
p.add_argument('--target-compiler',type=Path,help='Optional existing Xtensa GCC8.4 compiler; validates development HID ELFs')
a=p.parse_args();out=a.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
product=a.product.resolve();utilities=a.utilities.resolve();runtime=a.runtime.resolve();sdk=a.sdk.resolve()
driver=product/'minimal/drivers/x4pro_gt911/driver.c';fixture=product/'minimal/test/gt911_test.c'
assert digest(driver)=='1bcf7360a473fd20aef62bc974ec0bc87e78303015512098902e89826b64d322','Requires selected GT911 0.1.8'
sys.path.insert(0,str(utilities/'scripts'))
import native_utility_build as native
import test_native_utc_utilities as native_tests
if (out/'stage').exists():shutil.rmtree(out/'stage')
headers=native.stage(ROOT,runtime,out/'stage',out)
peripherals=out/'native_utility_peripherals.h'
peripherals.write_text(replace((utilities/'test/native_apps/native_utility_peripherals.h').read_text(),
    'static bool nu_retain(void){assert(!nu_retained);',
    'static bool nu_retain(void){assert(!nu_retained&&recovery_issues>0&&recovery_retain_logs==1);'))
# Include the provider ABI once, with the staged application's canonical headers.
for name in ('RiscTouchPowerV1.h','RiscTouchI2cV2.h','RiscI2cBusV1.h','RiscPlatformClockV1.h','RiscProviderSyncV1.h','GardenPlatformV1.h','RiscHardwareConfigV1.h'):
    target=headers/name
    if target.exists():target.unlink()
    target.symlink_to(sdk/name)
for header in sdk.glob("*.h"):
    if not (headers/header.name).exists():(headers/header.name).symlink_to(header)
records=[]
scenarios=['two-contact','out-of-range','queue-gap','transient-read','ambiguous-ack','unlock-retained','close-refused']
provider_flags=['-DX4_GT911_FIXTURE='+json.dumps(str(fixture))]
common=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-missing-field-initializers']
catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
for sanitized in (False,True):
    variant='sanitized' if sanitized else 'normal';dest=out/variant;dest.mkdir(exist_ok=True)
    san=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    binary=dest/'consumer'
    run([os.environ.get('CC','cc'),*common,*san,*provider_flags,'-I'+str(headers),ROOT/'test/native_apps/x4_touch_recovery_test.c',driver,'-o',binary])
    for scenario in scenarios+['provider-fault','snapshot-failed']:
        with (dest/('consumer-'+scenario+'.log')).open('w') as log:run([binary,scenario],stdout=log,stderr=subprocess.STDOUT,env=env,timeout=20)
        records.append(dict(layer='consumer',variant=variant,scenario=scenario));print(records[-1],flush=True)
    for app in ('ble_touchpad','ble_buttons'):
        path=dest/(app+'-fixture.c');native_tests.transformed_fixture(app,path);text=path.read_text()
        text=replace(text,json.dumps(str(utilities/'test/native_apps/native_utility_peripherals.h')),json.dumps(str(peripherals)))
        text=replace(text,'static unsigned ticks,polls,grants,frames,subs,presents;', '''static unsigned ticks,polls,grants,frames,subs,presents;
static unsigned recovery_issues,recovery_retain_logs;
const char *hid_recovery_scenario(void){return getenv("X4_TOUCH_SCENARIO");}
unsigned hid_recovery_poll(void){return polls;}
void hid_recovery_queue_loss(void);
void hid_recovery_unlock_fault(void);''')
        text=replace(text,'assert(++diagnostic_lines<80);','''assert(++diagnostic_lines<2000);
 if(strstr(s,"stage=touch-input"))recovery_issues++;
 if(strstr(s,"stage=app-retain"))recovery_retain_logs++;''')
        text=replace(text,'directory=argv[1];','directory=NULL;')
        text=replace(text,'if(!hid_owned||(++touch_count%2)==1){polls++;','''if(!hid_owned||(++touch_count%2)==1){polls++;
 if(polls==30&&!strcmp(hid_recovery_scenario(),"queue-gap"))hid_recovery_queue_loss();''')
        text=replace(text,'static int32_t fake_next(void*c,uint64_t n,risc_touch_event_v1*e){(void)c;assert(n>0&&n<5&&subscribers[n].live);','''static int32_t fake_next(void*c,uint64_t n,risc_touch_event_v1*e){(void)c;assert(n>0&&n<5&&subscribers[n].live);
 if(n==1&&polls>=30&&!strcmp(hid_recovery_scenario(),"unlock-retained"))hid_recovery_unlock_fault();''')
        text=replace(text,'if(getenv("HID_RENDER_RAW_HOME")&&polls==100)s->buttons=RISC_TOUCH_BUTTON_PRIMARY;','''if(polls>=30&&polls<=39)s->buttons=RISC_TOUCH_BUTTON_PRIMARY;''')
        text=replace(text,'held_mouse=b;hid_mouse_reports++;','''if(b||x||y||w){assert(polls<30||polls>=60);}held_mouse=b;hid_mouse_reports++;''')
        text=replace(text,'held_mod=mods;held_key=keys[0];hid_keyboard_reports++;','''if(mods||keys[0]){assert(polls<30||polls>=60);}held_mod=mods;held_key=keys[0];hid_keyboard_reports++;''')
        text=replace(text,'app_main();if(nu_retained){nu_check_retained();return 0;}nu_check_motion();','''app_main();if(nu_retained){
 assert(!strcmp(hid_recovery_scenario(),"unlock-retained")||!strcmp(hid_recovery_scenario(),"adapter-close-refused"));
 assert(recovery_issues==1&&recovery_retain_logs==1);
 unsigned frozen_polls=polls,frozen_diags=diagnostic_lines;app_module_fini();
 assert(nu_frozen_ticks==ticks&&nu_frozen_grants==grants&&nu_frozen_ops==NU_OPS&&nu_frozen_frames==frames&&nu_frozen_subs==subs&&nu_frozen_reads==nu_reads&&nu_frozen_raster==nu_raster_hash());
 assert(frozen_polls==polls&&frozen_diags==diagnostic_lines);
 puts("Actual HID/shared adapter: first-cause before retention; all ownership and work frozen PASS");return 0;}
 assert(recovery_issues>=1&&recovery_issues<=2&&!recovery_retain_logs&&!launches);nu_check_motion();''')
        text=replace(text,'/* A rejected unsubscribe preserves the real provider token for retry. */','''if(n==1&&!strcmp(hid_recovery_scenario(),"adapter-close-refused"))return false;
/* A rejected unsubscribe preserves the real provider token for retry. */''')
        text=replace(text,'nu_check_motion();app_module_fini();','''nu_check_motion();app_module_fini();
 if(nu_retained){
 assert(!strcmp(hid_recovery_scenario(),"adapter-close-refused")&&recovery_issues==2&&recovery_retain_logs==1);
 unsigned frozen_polls=polls,frozen_diags=diagnostic_lines;app_module_fini();
 assert(nu_frozen_ticks==ticks&&nu_frozen_grants==grants&&nu_frozen_ops==NU_OPS&&nu_frozen_frames==frames&&nu_frozen_subs==subs&&nu_frozen_reads==nu_reads&&nu_frozen_raster==nu_raster_hash());
 assert(frozen_polls==polls&&frozen_diags==diagnostic_lines);
 puts("Actual HID/shared adapter: close refusal retains subscription and freezes all later work PASS");return 0;}''')
        path.write_text(text)
        flags=native.flags(app)+['-DHID_RENDER_PAPER','-DHID_RENDER_WATCH_TOUCH']
        sources=native.sources(app,ROOT)+[path,catalog,ROOT/'test/native_apps/x4_hid_gt911_backend.c',driver]
        binary=dest/app
        run([os.environ.get('CC','cc'),*common,*san,*provider_flags,*flags,*['-I'+str(i) for i in native.include_paths(ROOT,headers)+[sdk]],*sources,'-Wl,--wrap=free','-lm','-o',binary])
        for scenario in scenarios[:-1]+['adapter-close-refused','hid-close-refused']:
            case=dest/(app+'-'+scenario);case.mkdir(exist_ok=True)
            actions=case/'actions.txt';actions.write_text('3 110 732\n'+''.join(f'{n} 150 350\n' for n in range(29,40))+'60 150 350\n')
            testenv=dict(env,X4_TOUCH_SCENARIO=scenario,HID_RENDER_ACTIVE='1',**({'HID_RENDER_MOUSE':'1'} if app=='ble_touchpad' else {'HID_RENDER_KEYS':'1'}))
            # Existing fixture refuses close/release/unsubscribe once and verifies exact retry counts.
            if scenario=='hid-close-refused':testenv['HID_RENDER_CLEANUP']='1'
            # Every scenario receives report loss; the close cases use malformed two-contact packets.
            with (case/'test.log').open('w') as log:run([binary,case,actions],stdout=log,stderr=subprocess.STDOUT,env=testenv,timeout=30)
            records.append(dict(layer=app,variant=variant,scenario=scenario));print(records[-1],flush=True)
receipt={'schema':1,'driver_version':'0.1.8','driver_sha256':digest(driver),'runs':records,'hardware_verified':False,'publication':'none','system_baseline':'7b175418e3063d9f4c571acf06c366144831b2af','utilities_fixture_revision':native.git(utilities,'rev-parse','HEAD'),'utilities_application_revision':'849581973a48daf07770446712a1c1fd4dfbc77c',
         'sources':{str(p):digest(p) for p in [driver,fixture,ROOT/'lib/PortableApps/include/PortableTouch.h',ROOT/'lib/PortableApps/src/native_custody_adapter.inc',ROOT/'lib/PortableApps/src/adapter.c',utilities/'Apps/ble_hid_app.inc',utilities/'test/native_apps/hid_renderer_test.c',utilities/'test/native_apps/native_utility_peripherals.h',ROOT/'test/native_apps/x4_touch_recovery_test.c',ROOT/'test/native_apps/x4_hid_gt911_backend.c',ROOT/'scripts/test_x4_touch_recovery.py',runtime/'sdk/app/RiscRuntimeV1.h']}}
if a.target_compiler:
    cc=str(a.target_compiler.resolve());version=subprocess.check_output([cc,'--version'],text=True).splitlines()[0]
    assert '8.4.0' in version and '2021r2-patch5' in version
    target=out/'target';target.mkdir(exist_ok=True)
    exports=target/'exports.map';exports.write_text('{ global: app_main; app_module_init; app_module_fini; local: *; };\n')
    validator=target/'validate-elf'
    run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(utilities/'test/native_apps/stubs'),'-I'+str(utilities/'lib/elf_loader/include'),utilities/'lib/elf_loader/src/esp_elf_validate.c',utilities/'test/native_apps/validate_test.c','-o',validator])
    modules=[]
    for app in ('ble_touchpad','ble_buttons'):
        elf=target/(app+'.elf');defines=native.flags(app)
        run([cc,'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wl,--version-script='+str(exports),'-Wall','-Wextra','-Werror',*defines,*['-I'+str(i) for i in native.include_paths(ROOT,headers)],*native.sources(app,ROOT),catalog,'-lgcc','-o',elf])
        syms=subprocess.check_output([cc.removesuffix('gcc')+'nm','-D',elf],text=True)
        imports={line.split()[-1] for line in syms.splitlines() if ' U ' in ' '+line}
        exported={line.split()[-1] for line in syms.splitlines() if len(line.split())>=3 and line.split()[-2] in ('T','D','B','R')}
        assert imports<=native.IMPORTS and exported==native.EXPORTS,(imports,exported)
        run([validator,elf]);modules.append(dict(app=app,bytes=elf.stat().st_size,sha256=digest(elf),imports=sorted(imports),defines=defines))
    receipt['target']={'compiler':version,'modules':modules,'validation':'Xtensa ABI, imports/exports, structural loader validation passed','release_versions':'allocated by central integration; these are development ELFs'}
(out/'evidence.json').write_text(json.dumps(receipt,indent=2)+'\n')
print('X4 touch report recovery: '+str(len(records))+' cases passed; physical hardware not tested')

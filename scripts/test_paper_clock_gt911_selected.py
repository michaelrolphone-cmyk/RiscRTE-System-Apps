#!/usr/bin/env python3
"""Actual selected .57 sparse Home flags with real GT911 and host providers."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for name in ('source','product','target-command','target-receipt','sdk','output-dir'):
    p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--driver-version',required=True)
p.add_argument('--driver-sha256',required=True)
p.add_argument('--require-fixed',action='store_true')
a=p.parse_args();source=a.source.resolve();product=a.product.resolve();out=a.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
command=json.loads(a.target_command.read_text());receipt=json.loads(a.target_receipt.read_text())
assert '--raster-snapshot' in command and '-DPORTABLE_RASTER_SNAPSHOT' in receipt['build_defines']
def argument(key):return Path(command[command.index(key)+1])
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
assert digest(source/'lib/PortableApps/src/adapter.c')==receipt['ble_broadcast']['source_sha256']['lib/PortableApps/src/adapter.c']
driver=product/'minimal/drivers/x4pro_gt911/driver.c';physical=product/'minimal/test/gt911_test.c';manifest=driver.with_name('manifest.json')
assert digest(driver)==a.driver_sha256 and json.loads(manifest.read_text())['version']==a.driver_version
inc=out/'include';shutil.copytree(receipt['paper_transition']['compiled_include_directory'],inc,dirs_exist_ok=True)
for h in (source/'lib/PortableApps/include').glob('*.h'):
    if not h.name.startswith(('Risc','AlarmService')):shutil.copyfile(h,inc/h.name)
for h in a.sdk.resolve().glob('*.h'):
    if not (inc/h.name).exists():shutil.copyfile(h,inc/h.name)
shutil.copytree(source/'lib/PortableApps/time',out/'time',dirs_exist_ok=True)
production_flags=[f for f in receipt['build_defines'] if f.startswith('-D')]
flags=[*production_flags,'-DTEST_CLOCK_ORDERED_INPUT','-DTEST_NATIVE_LANDSCAPE','-DTEST_HOME_DESK_LOCK',
       '-DX4_GT911_FIXTURE='+json.dumps(str(physical)),
       '-DCLOCK_ADAPTER_SOURCE='+json.dumps(str(source/'lib/PortableApps/src/adapter.c'))]
sources=[source/'Apps/paper_clock.c',ROOT/'test/native_apps/paper_clock_gt911_selected_adapter.c',
         ROOT/'test/native_apps/paper_clock_gt911_selected_test.c',ROOT/'test/native_apps/paper_clock_gt911_backend.c',driver]
sources += [source/'lib/PortableApps/src'/n for n in ['desk_clock_faces.c','PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c','quick_actions.c','quick_render.c','quick_session.c','quick_radios.c']]
sources += [argument('--local-sleep-source'),argument('--x4-idle-source')]
assert digest(sources[-2])==receipt['local_sleep_source_sha256']
incs=[inc,source/'lib/NativeApps/include',argument('--retained-wake-sdk').parent/'driver',argument('--local-sleep-source').parent.parent/'drivers/x4pro_power']
runs=[]
for san in (False,True):
    binary=out/('sanitized' if san else 'normal')
    extra=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
    compile_command=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-missing-field-initializers',*extra,*flags,*['-I'+str(i) for i in incs],*map(str,sources),'-o',str(binary)]
    (out/(binary.name+'-compile.json')).write_text(json.dumps(compile_command,indent=2)+'\n')
    subprocess.run(compile_command,check=True)
    for mode in ('fast','slow','busy'):
        for name,scene,repeat in [('single-report-cancel',15,False),('later-unchanged-report',15,True),
                                  ('rapid-swipe',14,False),('rapid-block-tap',16,False),
                                  ('independent-earlier-swipe',18,True),('launch-refusal-retry',9,False)]:
            state=out/'absent-state';state.unlink(missing_ok=True)
            result=subprocess.run([str(binary),str(scene),mode,str(state),*(['repeat'] if repeat else [])],
                                  capture_output=True,text=True,timeout=30,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
            log=out/(name+'-'+mode+'-'+str(int(san))+'.log');log.write_text(result.stdout+result.stderr)
            if result.returncode:
                assert name in ('single-report-cancel','later-unchanged-report'),(name,mode,result.stdout,result.stderr)
                assert "Assertion `launches==expected_launches' failed" in result.stderr,result.stderr
            runs.append({'name':name,'display':mode,'sanitized':san,'returncode':result.returncode,
                         'status':'PASS' if result.returncode==0 else 'FAIL','log':log.name})
            print('selected',name,mode,'ASan+UBSan' if san else 'normal',runs[-1]['status'],flush=True)
inputs=[*sources,physical,manifest,a.target_command,a.target_receipt,
        ROOT/'test/native_apps/paper_clock_gt911_report_test.c',ROOT/'test/native_apps/paper_clock_test.c',
        ROOT/'test/native_apps/sparse_clock_startup_test.c',Path(__file__).resolve()]
# The selected source is immutable; hash all production inputs, not only direct C files.
production=[*source.glob('Apps/*.inc'),source/'Apps/paper_clock.c']+[f for d in ('lib/PortableApps','lib/NativeApps/include') for f in (source/d).rglob('*') if f.is_file()]
evidence={'runs':runs,'driver_version':a.driver_version,'production_flags':production_flags,'omitted_production_flags':[],
 'source_sha256':{str(f):digest(f) for f in inputs},'production_source_sha256':{str(f.relative_to(source)):digest(f) for f in production},
 'staged_sdk_sha256':{str(f.relative_to(inc)):digest(f) for f in inc.rglob('*') if f.is_file()},
 'provider_boundary':'Actual selected GT911; synthetic Runtime, display, alarm, storage, and optional-service providers',
 'optional_service_configuration':'Contexts unavailable, BLE defaults off, healthy empty crash spool',
 'hardware_tested':False,'production_modified':False}
(out/'evidence.json').write_text(json.dumps(evidence,indent=2)+'\n')
if a.require_fixed:assert all(r['status']=='PASS' for r in runs),'Selected Clock regression remains'

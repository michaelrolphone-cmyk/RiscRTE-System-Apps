#!/usr/bin/env python3
"""Build one complete Home/Clock ELF from both current owning semantic views.
No device, display resolution, physical instance, or product selector is accepted.
"""
import argparse,hashlib,json,os,pathlib,shlex,shutil,subprocess,sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
def run():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('watch','watch-system','watch-utilities','runtime','reader','interfaces','alarm-utilities','catalog','compiler','legacy-linker','output'):
  p.add_argument('--'+name,type=pathlib.Path,required=True)
 a=p.parse_args()
 for name,value in vars(a).items():setattr(a,name,value.resolve())
 out=a.output;out.mkdir(parents=True,exist_ok=False)
 compiler=a.compiler;linker=a.legacy_linker
 version=subprocess.check_output([str(compiler),'--version'],text=True)
 if '15.2.0' not in version or 'esp-15.2.0_20251204' not in version:raise ValueError('Pinned common compiler required')
 bridge=out/'linker-only';bridge.mkdir()
 shim=bridge/'ld'
 shim.write_text('#!/usr/bin/env python3\nimport os,sys\na=sys.argv[1:]\nassert not any(x.startswith("--dynconfig=") and x!="--dynconfig=xtensa_esp32s3.so" for x in a)\nos.execv('+repr(str(linker))+',['+repr(str(linker))+']+[x for x in a if x!="--dynconfig=xtensa_esp32s3.so"])\n');shim.chmod(0o755)
 (bridge/(compiler.name.removesuffix('gcc')+'ld')).symlink_to(shim)
 bin=out/'compiler';bin.mkdir()
 for language in ('gcc','g++'):
  wrapper=bin/(compiler.name.removesuffix('gcc')+language)
  actual=compiler.parent/wrapper.name
  wrapper.write_text('#!/bin/sh\nexec '+shlex.quote(str(actual))+' -B'+shlex.quote(str(bridge)+'/')+' "$@"\n');wrapper.chmod(0o755)
 for tool in ('nm','objcopy','readelf','objdump'):
  target=bin/(compiler.name.removesuffix('gcc')+tool);target.symlink_to(compiler.parent/target.name)
 cc=bin/compiler.name
 sdk=out/'overview-sdk';shutil.copytree(a.reader/'sdk/driver',sdk)
 shutil.copytree(a.interfaces,sdk,dirs_exist_ok=True)
 # The established public power contracts are staged explicitly, never the
 # experimental same-name Reader lease header.
 for name in ('RiscPowerSleepV1.h','RiscDisplayPowerV1.h','RiscPowerKeyV1.h'):
  shutil.copyfile(a.watch/'sdk/driver'/name,sdk/name)
 commands=[]
 watch=[sys.executable,str(a.watch/'scripts/build.py'),'--system',str(a.watch_system),'--utilities',str(a.watch_utilities),'--output',str(out/'faces-stage'),'--cc',str(cc),'--role','default','--shared-view-output',str(out/'faces'),'--shared-system',str(ROOT)]
 commands.append(watch);subprocess.run(watch,check=True)
 host=['--desk-clock','--sparse-start','--desk-lock-home','--desk-points-face','--navigation','--alarm-client','--wake-light-restore','--display-rotation','0',
 '--local-sleep-source',ROOT/'lib/PortableApps/src/PortableHomeSleep.c','--sleep-capability','power.sleep','--sleep-sdk',sdk,'--retained-wake-sdk',a.runtime/'sdk/app',
 '--tagged-alarm-utilities',a.alarm_utilities,'--paper-crossfade','--paper-display-sdk',sdk,'--paper-transitions','--quick-actions','--quick-radios','--quick-usb-transfer',
 '--resident-shell-host','--resident-runtime-sdk',a.runtime/'sdk/app','--ble-broadcast','--shared-idle-source',ROOT/'lib/PortableApps/src/PortableHomeIdle.c',
 '--contexts-rf-only','--resident-legacy-handoff','--resident-policy','--resident-loading-catalog',a.catalog,
 '--raster-snapshot','--frontlight-tone','--frontlight-tone-sdk',sdk,'--display-settled-sdk',sdk,'--crash-report-sd',
 '--boot-animation','--relocation-tools',a.reader/'scripts','--stage-logs','--installed-catalog-volume-instance','0','--deferred-foreground-services',
 '--experimental-atomic-quick','--experimental-atomic-home-automatic','--experimental-settings-availability',
 '--shared-radio-source',a.watch/'apps/clock/quick_radios.c','--shared-view-output',out/'overview','--output-dir',out/'overview-stage']
 overview=list(map(str,[sys.executable,ROOT/'scripts/build_paper_clock.py',*host]));commands.append(overview)
 subprocess.run(overview,check=True,env=dict(os.environ,NATIVE_APP_CC=str(cc)))
 # The common front controller must use one staged Runtime identity too.
 # Quoted includes in PortableHomeBoot must not reopen its frozen local copy.
 common_sdk=out/'common-sdk';shutil.copytree(a.runtime/'sdk/app',common_sdk)
 from shared_home_sdk import stage_home_headers
 common_closure=stage_home_headers(ROOT,common_sdk,roots=('RiscRuntimeV1.h','RiscResidentShellV1.h','PortableHomeBoot.h','PortableHomeSelection.h','PortableHomeBindings.h'))
 if (common_sdk/'RiscRuntimeV1.h').read_bytes()!=(a.runtime/'sdk/app/RiscRuntimeV1.h').read_bytes():raise ValueError('Common Runtime SDK identity changed')
 common_records=[]
 common_objects=[]
 for index,source in enumerate((ROOT/'Apps/shared_home.c',ROOT/'lib/PortableApps/src/PortableHomeBoot.c')):
  obj=out/('common-'+str(index)+'.o');dep=out/('common-'+str(index)+'.d')
  compile_command=[str(cc),'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-Wall','-Wextra','-Werror','-I'+str(common_sdk),'-MMD','-MF',str(dep),'-c',str(source),'-o',str(obj)]
  commands.append(compile_command);subprocess.run(compile_command,check=True)
  dependency_text=dep.read_text().replace('\\\n',' ')
  paths=[pathlib.Path(name) for name in shlex.split(dependency_text.split(':',1)[1])]
  common_records.append({'source':str(source),'command':compile_command,'dependencies':{str(path):hashlib.sha256(path.read_bytes()).hexdigest() for path in paths},'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()})
  common_objects.append(obj)
 (out/'common-sdk.json').write_text(json.dumps({'staged_closure':common_closure,'translation_units':common_records},indent=2)+'\n')
 exports={'app_main','app_module_init','app_module_fini','risc_resident_app_descriptor_v1'}
 mapping=out/'exports.map';mapping.write_text('{ global: '+'; '.join(sorted(exports))+'; local: *; };\n')
 elf=out/'default.elf'
 command=[str(cc),'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-Wl,--gc-sections','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wl,--version-script='+str(mapping),'-Wl,-Map='+str(out/'default.map')+',--cref','-Wall','-Wextra','-Werror',
 *map(str,common_objects),str(out/'faces/view.o'),str(out/'overview/view.o'),'-lgcc','-o',str(elf)]
 commands.append(command);subprocess.run(command,check=True)
 sys.path.insert(0,str(a.watch/'scripts'))
 from normalize_xtensa_relocations import normalize
 from compact_current_elf import compact
 normalize(elf);compaction=compact(elf,str(cc),debug_path=out/'debug/default.elf')
 # Retained target section/symbol evidence; diagnostics never change the ELF.
 reports={}
 for label,path in (('complete',out/'debug/default.elf'),('faces',out/'faces/view-unprefixed.o'),('overview',out/'overview/view-unprefixed.o')):
  for kind,args in (('symbols',['nm','-S','--size-sort','--radix=d']),('sections',['readelf','-SW'])):
   text=subprocess.check_output([str(cc).removesuffix('gcc')+args[0],*args[1:],str(path)],text=True)
   report=out/(label+'-'+kind+'.txt');report.write_text(text);reports[report.name]=hashlib.sha256(report.read_bytes()).hexdigest()
   print('TARGET SIZE REPORT '+label+' '+kind+'\n'+text)
 (out/'size-reports.json').write_text(json.dumps(reports,sort_keys=True,indent=2)+'\n')
 validator=out/'validate-elf'
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'),str(ROOT/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
 subprocess.run([str(validator),str(elf)],check=True)
 symbols=subprocess.check_output([str(cc).removesuffix('gcc')+'nm','-D',str(elf)],text=True)
 actual={r.split()[-1] for r in symbols.splitlines() if len(r.split())>=3 and r.split()[-2] in ('T','D','B','R')}
 imports={r.split()[-1] for r in symbols.splitlines() if ' U ' in ' '+r}
 allowed={'risc_runtime_get_api','memcpy','memset','memcmp','memmove','strcmp','strncmp','strlen','snprintf','malloc','calloc','free','strcpy','memchr','strrchr'}
 if actual!=exports or not imports<=allowed:raise ValueError((actual,imports-allowed))
 manifest_path=ROOT/'Apps/shared_home.json'
 manifest=json.loads(manifest_path.read_text())
 declarations=manifest['requires']+manifest['optional']
 if len(declarations)>32 or len({d['capability'] for d in declarations})!=len(declarations):raise ValueError('Common declaration capacity/identity mismatch')
 (out/'default.json').write_text(json.dumps(manifest,indent=2)+'\n')
 raw=elf.read_bytes();digest=hashlib.sha256(raw).hexdigest()
 receipt={'schema':1,'version':'1.0.0','shared_elf':digest,'size_bytes':len(raw),'views':['native-faces','overview-retained-desk'],'commands':commands,'imports':sorted(imports),'exports':sorted(actual),'compiler':version,'compiler_sha256':hashlib.sha256(compiler.read_bytes()).hexdigest(),'linker_sha256':hashlib.sha256(linker.read_bytes()).hexdigest(),'compaction':compaction,'physical_testing':False,'common_sdk_receipt_sha256':hashlib.sha256((out/'common-sdk.json').read_bytes()).hexdigest(),'manifest_sha256':hashlib.sha256((out/'default.json').read_bytes()).hexdigest(),'manifest_source_sha256':hashlib.sha256(manifest_path.read_bytes()).hexdigest(),'declared_capabilities':len(declarations)}
 (out/'build.json').write_text(json.dumps(receipt,indent=2)+'\n')
 print('Complete shared Home target structural/import/export qualification PASS',digest,len(raw))
if __name__=='__main__':run()

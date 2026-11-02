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
 exports={'app_main','app_module_init','app_module_fini','risc_resident_app_descriptor_v1'}
 mapping=out/'exports.map';mapping.write_text('{ global: '+'; '.join(sorted(exports))+'; local: *; };\n')
 elf=out/'default.elf'
 command=[str(cc),'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-Wl,--gc-sections','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wl,--version-script='+str(mapping),'-Wall','-Wextra','-Werror',
 *['-I'+str(d) for d in (a.runtime/'sdk/app',ROOT/'sdk/app',ROOT/'lib/PortableApps/include')],
 str(ROOT/'Apps/shared_home.c'),str(ROOT/'lib/PortableApps/src/PortableHomeBoot.c'),str(out/'faces/view.o'),str(out/'overview/view.o'),'-lgcc','-o',str(elf)]
 commands.append(command);subprocess.run(command,check=True)
 sys.path.insert(0,str(a.watch/'scripts'))
 from normalize_xtensa_relocations import normalize
 from compact_current_elf import compact
 normalize(elf);compaction=compact(elf,str(cc),debug_path=out/'debug/default.elf')
 validator=out/'validate-elf'
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'),str(ROOT/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
 subprocess.run([str(validator),str(elf)],check=True)
 symbols=subprocess.check_output([str(cc).removesuffix('gcc')+'nm','-D',str(elf)],text=True)
 actual={r.split()[-1] for r in symbols.splitlines() if len(r.split())>=3 and r.split()[-2] in ('T','D','B','R')}
 imports={r.split()[-1] for r in symbols.splitlines() if ' U ' in ' '+r}
 allowed={'risc_runtime_get_api','memcpy','memset','memcmp','memmove','strcmp','strncmp','strlen','snprintf','malloc','calloc','free','strcpy','memchr','strrchr'}
 if actual!=exports or not imports<=allowed:raise ValueError((actual,imports-allowed))
 raw=elf.read_bytes();digest=hashlib.sha256(raw).hexdigest()
 receipt={'schema':1,'version':'1.0.0','shared_elf':digest,'size_bytes':len(raw),'views':['native-faces','overview-retained-desk'],'commands':commands,'imports':sorted(imports),'exports':sorted(actual),'compiler':version,'compiler_sha256':hashlib.sha256(compiler.read_bytes()).hexdigest(),'linker_sha256':hashlib.sha256(linker.read_bytes()).hexdigest(),'compaction':compaction,'physical_testing':False}
 (out/'build.json').write_text(json.dumps(receipt,indent=2)+'\n')
 print('Complete shared Home target structural/import/export qualification PASS',digest,len(raw))
if __name__=='__main__':run()

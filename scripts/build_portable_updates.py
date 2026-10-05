#!/usr/bin/env python3
"""Build ordinary update providers and existing shared apps; no publication."""
import argparse,hashlib,json,os,shutil,subprocess,struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def native_update_manifest(name):
 if name not in ('ota_update','app_store'):raise ValueError('Unknown native update app')
 manifest=json.loads((ROOT/'Apps/native'/(name+'.json')).read_text())
 if manifest.get('id')!=name or manifest.get('file_name')!=name+'.elf' or manifest.get('type')!='application':raise ValueError('Native update manifest identity mismatch')
 return manifest

def build(args):
 cc=os.environ.get('NATIVE_APP_CC') or shutil.which('xtensa-esp32s3-elf-gcc')
 if not cc:
  cc=str(Path(os.environ.get('PLATFORMIO_CORE_DIR',Path.home()/'.platformio'))/'packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc')
 cxx=cc.removesuffix('gcc')+'g++';nm=cc.removesuffix('gcc')+'nm'
 out=args.output_dir;out.mkdir(parents=True,exist_ok=True)
 common=['-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-Wl,--hash-style=sysv','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include')]
 validator=out/'validate-elf'
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'),str(ROOT/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
 def compile_artifact(name,compiler,sources,flags,exports,manifest):
  dest=out/name;dest.mkdir(exist_ok=True);mapping=dest/'exports.map';mapping.write_text('{ global: '+ '; '.join(sorted(exports))+'; local: *; };\n')
  elf=dest/manifest['file_name'];subprocess.run([compiler,*common,*flags,'-Wl,--version-script='+str(mapping),*map(str,sources),'-o',str(elf)],check=True)
  symbols=subprocess.check_output([nm,'-D',str(elf)],text=True)
  imports={line.split()[-1] for line in symbols.splitlines() if ' U ' in ' '+line}
  allowed={'risc_runtime_get_api','memcpy','memset','memcmp','strcmp','strlen','snprintf','strcpy','malloc','calloc','free'}
  if name.startswith('software-update-'): allowed-={'malloc','calloc','free','risc_runtime_get_api'}
  if not imports<=allowed: raise ValueError('Unexpected imports: '+str(imports-allowed))
  actual={line.split()[-1] for line in symbols.splitlines() if len(line.split())>=3 and line.split()[-2] in ('T','D','B','R')}
  if actual!=exports: raise ValueError('Unexpected exports: '+str(actual))
  subprocess.run([str(validator),str(elf)],check=True)
  data=elf.read_bytes();shoff=struct.unpack_from('<I',data,32)[0];shsize,shnum,shstr=struct.unpack_from('<HHH',data,46)
  sections=[struct.unpack_from('<10I',data,shoff+i*shsize) for i in range(shnum)]
  names=data[sections[shstr][4]:sections[shstr][4]+sections[shstr][5]]
  sizes={names[s[0]:].split(b'\0',1)[0].decode():s[5] for s in sections}
  bss=sizes.get('.bss',0)
  if name.startswith('software-update-'):
   if not 512*1024<=bss<=704*1024:raise ValueError('Provider workspace must remain bounded ELF BSS in native PSRAM allocation')
   if any(sizes.get(s,0) for s in ('.init_array','.ctors')):raise ValueError('Provider must not require global constructors')
  stack_frames={}
  if name.startswith('software-update-'):
   probe=dest/'stack-probe.o'
   compile_flags=[f for f in common if f not in ('-shared','-Wl,--hash-style=sysv')]
   subprocess.run([compiler,*compile_flags,*flags,'-fstack-usage','-c',str(sources[0]),'-o',str(probe)],check=True)
   for line in probe.with_suffix('.su').read_text().splitlines():
    function,size,kind=line.split('\t');stack_frames[function]=int(size)
    if kind!='static' or int(size)>2048:raise ValueError('Provider per-function stack budget exceeded: '+line)
  (dest/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
  inputs=[p for d in ['Services/update','lib/PortableApps'] for p in (ROOT/d).rglob('*') if p.is_file()]+[ROOT/'Apps/update_portable.inc',ROOT/'scripts/build_portable_updates.py',ROOT/'lib/NativeApps/src/UnsignedDivisionCompat.c']+list(sources)+list((ROOT/'Apps/native').glob('*.json'))
  record={'purpose':'development-only-not-deployment','sha256':hashlib.sha256(elf.read_bytes()).hexdigest(),'size_bytes':elf.stat().st_size,'compiler':subprocess.check_output([compiler,'--version'],text=True).splitlines()[0],'imports':sorted(imports),'exports':sorted(exports),'build_defines':flags,'bss_bytes':bss,'section_sizes':sizes,'stack_frames':stack_frames,'source_sha256':{str(p.relative_to(ROOT)) if p.is_relative_to(ROOT) else p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}}
  (dest/'build-record.json').write_text(json.dumps(record,indent=2)+'\n')
  return elf
 for firmware,kind in [(1,'firmware'),(0,'apps')]:
  manifest=json.loads((ROOT/'Services/update'/kind/'manifest.json').read_text())
  compile_artifact('software-update-'+kind,cxx,[ROOT/'Services/update/service.cpp'],['-std=c++17','-fno-exceptions','-fno-rtti','-DUPDATE_FIRMWARE='+str(firmware)],{'t5_driver_get'},manifest)
 if args.services_only:return
 catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
 for firmware,name in [(1,'ota_update'),(0,'app_store')]:
  if args.app and name!=args.app:continue
  flags=['-std=c11','-DPORTABLE_UPDATE_APP','-DPORTABLE_UPDATE_FIRMWARE='+str(firmware),'-DPORTABLE_WIFI_INSTANCE='+str(args.wifi_instance),'-DPORTABLE_UPDATE_RTC_UTC_OFFSET_SECONDS='+str(args.rtc_utc_offset_seconds)]
  if args.nova_ui:flags+=['-DPORTABLE_NOVA_UI']
  if args.alarm_client:flags+=['-DPORTABLE_ALARM_CLIENT']
  if args.navigation:flags+=['-DPORTABLE_INPUT_NAVIGATION']
  if args.full_frames:flags+=['-DPORTABLE_FORCE_FULL_FRAMES']
  flags+=['-DPORTABLE_TOUCH_ROTATION='+str(args.touch_rotation)]
  manifest={'type':'application','id':name,'version':'1.1.0','architecture':'xtensa-esp32s3','file_name':name+'.elf','entry':'app_main','requires':[{'capability':c,'api':v} for c,v in [('display.output',1),('input.touch.raw',1),('rtc.clock',2),('storage.key-value',1),('net.wifi',1),('software.update.'+('firmware' if firmware else 'apps'),1)]]}
  if args.nova_ui:manifest=native_update_manifest(name)
  if args.alarm_client:manifest['requires'].append({'capability':'alarm.service','api':1})
  if args.navigation:manifest['requires'].append({'capability':'input.navigation','api':1})
  compile_artifact(name,cc,[ROOT/'Apps'/(name+'.c'),ROOT/'lib/PortableApps/src/adapter.c',catalog],flags,{'app_main','app_module_init','app_module_fini'},manifest)
  licenses=out/name/'licenses';licenses.mkdir(exist_ok=True)
  for source in [ROOT/'LICENSE',*list((ROOT/'lib/PortableApps/settings_fonts').glob('LICENSE-*'))]:shutil.copyfile(source,licenses/source.name)
 print('Portable update target ELFs, bounded imports/exports and structural validator passed')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output-dir',type=Path,default=ROOT/'dist/portable/updates');p.add_argument('--services-only',action='store_true');p.add_argument('--nova-ui',action='store_true');p.add_argument('--app',choices=['ota_update','app_store']);p.add_argument('--wifi-instance',type=int,default=0);p.add_argument('--rtc-utc-offset-seconds',type=int,required=True);p.add_argument('--alarm-client',action='store_true');p.add_argument('--navigation',action='store_true');p.add_argument('--full-frames',action='store_true');p.add_argument('--touch-rotation',type=int,choices=[0,180],default=0);build(p.parse_args())

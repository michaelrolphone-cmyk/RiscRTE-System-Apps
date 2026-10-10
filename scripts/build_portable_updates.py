#!/usr/bin/env python3
"""Build ordinary update providers and existing shared apps; no publication."""
import argparse,sys,hashlib,json,os,shutil,subprocess,struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
import portable_broadcast_build
import portable_idle_build
import portable_quick_build
import portable_native_toolbar_build
def native_update_manifest(name):
 if name not in ('ota_update','app_store'):raise ValueError('Unknown native update app')
 manifest=json.loads((ROOT/'Apps/native'/(name+'.json')).read_text())
 if manifest.get('id')!=name or manifest.get('file_name')!=name+'.elf' or manifest.get('type')!='application':raise ValueError('Native update manifest identity mismatch')
 return manifest

def product_policy(args, parser):
 product=getattr(args,'product','watch')
 url=getattr(args,'catalog_url',None)
 if product=='watch':
  if url:parser.error('--catalog-url is only supported for explicit X4 policy')
  if portable_native_toolbar_build.selected(args):parser.error('X4 native time requires --product x4')
  return [],{'product':'twatch-s3','feed':'watch-default'}
 if not portable_native_toolbar_build.selected(args) and not args.services_only:parser.error('X4 apps require --time-profile x4-native-time')
 expected='https://raw.githubusercontent.com/michaelrolphone-cmyk/RiskRTE-XTEINK-X4-PRO/release-index/release-index.json'
 if url and url!=expected:parser.error('X4 catalog must use its explicit product repository release-index')
 flags=['-DUPDATE_PRODUCT_X4']
 if url:flags+=['-DUPDATE_CATALOG_URL="'+url+'"']
 return flags,{'product':'xteink-x4-pro','repository':'michaelrolphone-cmyk/RiskRTE-XTEINK-X4-PRO','catalog_url':url or '', 'feed_configured':bool(url),'runtime_only':False}

def build(args,parser=None):
 parser=parser or argparse.ArgumentParser()
 portable_native_toolbar_build.validate(args,parser)
 scrolling=getattr(args,"touch_scrolling",False)
 if scrolling and (args.product!="x4" or not portable_native_toolbar_build.selected(args) or (not args.paper_transitions and not args.resident_shell_client)):parser.error("--touch-scrolling requires X4 native time and paper transitions")
 portable_broadcast_build.validate(args,parser,portable_native_toolbar_build.selected(args))
 service_flags,policy=product_policy(args,parser)
 if not portable_native_toolbar_build.selected(args) and not args.services_only and args.rtc_utc_offset_seconds is None:parser.error('Watch apps require --rtc-utc-offset-seconds')
 if portable_native_toolbar_build.selected(args) and args.rtc_utc_offset_seconds is not None:parser.error('Native UTC cannot use an RTC offset')
 cc=os.environ.get('NATIVE_APP_CC') or shutil.which('xtensa-esp32s3-elf-gcc')
 if not cc:
  cc=str(Path(os.environ.get('PLATFORMIO_CORE_DIR',Path.home()/'.platformio'))/'packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc')
 cxx=cc.removesuffix('gcc')+'g++';nm=cc.removesuffix('gcc')+'nm'
 out=args.output_dir;out.mkdir(parents=True,exist_ok=True)
 quick_flags,quick_sources=([],[]) if (portable_idle_build.selected(args) or args.resident_shell_client) else portable_quick_build.configure(args,parser or argparse.ArgumentParser(),ROOT,out)
 common=['-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-Wl,--hash-style=sysv','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include')]
 validator=out/'validate-elf'
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'),str(ROOT/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
 def compile_artifact(name,compiler,sources,flags,exports,manifest,includes=None,native_receipt=None):
  dest=out/name;dest.mkdir(exist_ok=True);mapping=dest/'exports.map';mapping.write_text('{ global: '+ '; '.join(sorted(exports))+'; local: *; };\n')
  elf=dest/manifest['file_name'];subprocess.run([compiler,*(['-I'+str(includes)] if includes else []),*common,*flags,'-Wl,--version-script='+str(mapping),*map(str,sources),'-o',str(elf)],check=True)
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
  if manifest['type']=='application':(dest/(name+'.json')).write_text(json.dumps(manifest,indent=2)+'\n')
  inputs=[p for d in ['Services/update','lib/PortableApps'] for p in (ROOT/d).rglob('*') if p.is_file()]+[ROOT/'Apps/update_portable.inc',ROOT/'Apps/update_paper.inc',ROOT/'Apps/update_scroll.inc',ROOT/'Apps/PaperPresentation.h',ROOT/'Apps/PaperFrame.h',ROOT/'scripts/build_portable_updates.py',ROOT/'scripts/portable_quick_build.py',ROOT/'scripts/portable_native_toolbar_build.py',ROOT/'scripts/portable_alarm_build.py',ROOT/'scripts/portable_idle_build.py',ROOT/'lib/NativeApps/src/UnsignedDivisionCompat.c']+list(sources)+list((ROOT/'Apps/native').glob('*.json'))
  record={'purpose':'development-only-not-deployment','repository_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'working_tree_dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True).strip()),'version':manifest['version'],'update_policy':policy,'notes':['Capability-selected paper UI shares existing controller; no product backend qualification','Home and Quick Wi-Fi handoffs require successful update cleanup; local Back returns to springboard.elf','Quick Actions requires KV namespace 1; saved Wi-Fi requires namespace 6; provider instances remain explicit owner grants'],'sha256':hashlib.sha256(elf.read_bytes()).hexdigest(),'size_bytes':elf.stat().st_size,'compiler':subprocess.check_output([compiler,'--version'],text=True).splitlines()[0],'imports':sorted(imports),'exports':sorted(exports),'build_defines':flags,'bss_bytes':bss,'section_sizes':sizes,'stack_frames':stack_frames,'source_sha256':{str(p.relative_to(ROOT)) if p.is_relative_to(ROOT) else p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}}
  if scrolling and native_receipt:record["touch_scrolling"]={"version":1,"bounded_viewport":True,"momentum":True,"completed_frame_identity":True,"confirmation_revalidated":True}
  portable_native_toolbar_build.record(args,record,manifest,native_receipt)
  if native_receipt:
   portable_idle_build.record(args,record,flags)
   portable_broadcast_build.record(args,ROOT,record,manifest,flags)
  if manifest['type']=='application':portable_quick_build.record(args,record)
  (dest/'build-record.json').write_text(json.dumps(record,indent=2)+'\n')
  portable_native_toolbar_build.write_admission(ROOT,dest,manifest,record)
  notices=dest/'licenses';notices.mkdir(exist_ok=True);shutil.copyfile(ROOT/'LICENSE',notices/'System-Apps-LICENSE.txt')
  return elf
 for firmware,kind in ([] if getattr(args,'apps_only',False) else [(1,'firmware'),(0,'apps')]):
  manifest=json.loads((ROOT/'Services/update'/kind/'manifest.json').read_text())
  routes=bool(firmware and getattr(args,'source_routes',False))
  if routes:manifest['version']='0.1.5'
  compile_artifact('software-update-'+kind,cxx,[ROOT/'Services/update/service.cpp'],['-std=c++17','-fno-exceptions','-fno-rtti','-DUPDATE_FIRMWARE='+str(firmware),*service_flags,*(['-DUPDATE_SOURCE_ROUTES=1'] if routes else [])],{'t5_driver_get'},manifest)
 if args.services_only:return
 catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
 for firmware,name in [(1,'ota_update'),(0,'app_store')]:
  if args.app and name!=args.app:continue
  dest=out/name;dest.mkdir(exist_ok=True)
  includes,native_flags,native_sources,native_receipt=portable_native_toolbar_build.configure(args,parser,ROOT,dest,name)
  if portable_idle_build.selected(args) or args.resident_shell_client:
   quick_flags,quick_sources=portable_quick_build.configure(args,parser,ROOT,dest,includes)
   if portable_idle_build.selected(args):includes=Path(args.x4_idle_receipt['compiled_include_directory'])
  flags=['-std=c11','-DPORTABLE_UPDATE_APP','-DUPDATE_RETURN_APP="springboard.elf"',*quick_flags,'-DPORTABLE_UPDATE_FIRMWARE='+str(firmware),'-DPORTABLE_WIFI_INSTANCE='+str(args.wifi_instance),*native_flags]
  if native_receipt:
   if not policy['feed_configured']:flags+=['-DPORTABLE_UPDATE_FEED_DISABLED']
  else:flags+=['-DPORTABLE_UPDATE_RTC_UTC_OFFSET_SECONDS='+str(args.rtc_utc_offset_seconds)]
  if scrolling:flags+=['-DPORTABLE_TOUCH_SCROLL','-DPORTABLE_APP_TOUCH_SCROLL','-DPORTABLE_UPDATE_TOUCH_SCROLL']
  if args.nova_ui:flags+=['-DPORTABLE_NOVA_UI']
  if args.alarm_client:flags+=['-DPORTABLE_ALARM_CLIENT']
  if args.navigation:flags+=['-DPORTABLE_INPUT_NAVIGATION']
  if args.full_frames:flags+=['-DPORTABLE_FORCE_FULL_FRAMES']
  flags+=['-DPORTABLE_TOUCH_ROTATION='+str(args.touch_rotation),'-DPORTABLE_DISPLAY_ROTATION='+str(args.display_rotation)]
  manifest={'type':'application','id':name,'version':'1.1.0','architecture':'xtensa-esp32s3','file_name':name+'.elf','entry':'app_main','requires':[{'capability':c,'api':v} for c,v in [('display.output',1),('input.touch.raw',1),('rtc.clock',2),('storage.key-value',1),('net.wifi',1),('software.update.'+('firmware' if firmware else 'apps'),1)]]}
  if args.nova_ui:manifest=native_update_manifest(name)
  if native_receipt:
   native_receipt['version']=portable_quick_build.version(args,name,portable_idle_build.version(args,name,portable_broadcast_build.version(args,name,native_receipt['version'])))
   manifest['version']=native_receipt['version']
  flags+=portable_broadcast_build.flags(args)
  portable_quick_build.requirements(args,manifest['requires'])
  if args.alarm_client:manifest['requires'].append({'capability':'alarm.service','api':1})
  if args.navigation:manifest['requires'].append({'capability':'input.navigation','api':1})
  manifest['requires']=list({(r['capability'],r['api']):r for r in manifest['requires']}.values())
  portable_native_toolbar_build.requirements(args,manifest['requires'])
  portable_broadcast_build.requirements(args,manifest['requires'])
  compile_artifact(name,cc,[ROOT/'Apps'/(name+'.c'),ROOT/'lib/PortableApps/src/adapter.c',catalog,*quick_sources,*native_sources],flags,portable_quick_build.exports(args,{'app_main','app_module_init','app_module_fini'}),manifest,includes,native_receipt)
  licenses=out/name/'licenses';licenses.mkdir(exist_ok=True)
  for source in [ROOT/'LICENSE',*[p for group in ('settings_fonts','paper_fonts','fonts') for p in (ROOT/'lib/PortableApps'/group).glob('LICENSE-*')]]:shutil.copyfile(source,licenses/source.name)
 print('Portable update target ELFs, bounded imports/exports and structural validator passed')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);parser=p;p.add_argument("--touch-scrolling",action="store_true",help="Select paper touch/momentum update catalogs with exact release confirmation");portable_broadcast_build.options(p);portable_quick_build.options(p);p.add_argument('--output-dir',type=Path,default=ROOT/'dist/portable/updates');group=p.add_mutually_exclusive_group();group.add_argument('--services-only',action='store_true');group.add_argument('--apps-only',action='store_true',help='Build application ELFs without rebuilding update provider modules');p.add_argument('--nova-ui',action='store_true');p.add_argument('--app',choices=['ota_update','app_store']);p.add_argument('--wifi-instance',type=int,default=0);p.add_argument('--rtc-utc-offset-seconds',type=int);p.add_argument('--product',choices=['watch','x4'],default='watch');p.add_argument('--catalog-url');p.add_argument('--source-routes',action='store_true',help='Select exact-source firmware routing provider0.1.5');portable_native_toolbar_build.options(p);p.add_argument('--alarm-client',action='store_true');p.add_argument('--navigation',action='store_true');p.add_argument('--full-frames',action='store_true');p.add_argument('--display-rotation',type=int,choices=[0,90],default=None);p.add_argument('--touch-rotation',type=int,choices=[0,180],default=0);build(p.parse_args(),p)

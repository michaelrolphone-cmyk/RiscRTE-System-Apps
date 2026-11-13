#!/usr/bin/env python3
"""Link a previously qualified canonical Files object cohort; no product writes.
Requires exact background-owner objects and an explicit reviewed app version.
Unresolved integration hooks fail the import audit; no stub substitutions.
"""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
from native_toolchain import compiler_path
from portable_file_sharing_build import MODULES
from compact_current_elf import compact
ROOT=Path(__file__).resolve().parents[1]
def make_manifest(version,alarm_api):
 optional=[('ui.scene',1),('ui.text-input',1),('storage.installed-files',1),('storage.app-data.export',1),('storage.volume',1),('file.open',1),('file.open.source',1),('network.tcp.listener',1),('crypto.entropy',1),('net.wifi',1),('bluetooth.hci',1),('bluetooth.session-setup',1),('storage.key-value',1),('storage.key-value.atomic',1),('contexts.service',1),('storage.shared-data',1),('telemetry.broadcast',1),('rtc.clock',2),('board.battery',1),('alarm.service',alarm_api),('display.output',1),('input.touch.raw',1),('input.navigation',1),('input.power-key',1),('power.sleep',1),('display.power',1),('motion.accel',1),('app.catalog',1),('runtime.realtime',1)]
 assert len(optional)<=32
 manifest={'type':'application','id':'file_browser','version':version,'architecture':'xtensa-esp32s3','file_name':'file_browser.elf','entry':'app_main','display_name':'Files','icon':'solid:f07c','requires':[],'optional':[{'capability':n,'api':v} for n,v in optional]}
 return manifest

def verified_linked_manifest(manifest_bytes,receipt):
 manifest=json.loads(manifest_bytes)
 if hashlib.sha256(manifest_bytes).hexdigest()!=receipt['manifest_sha256']:
  raise ValueError('Linked manifest bytes differ from receipt')
 if manifest['version']!=receipt['version']:
  raise ValueError('Linked package version differs from receipt')
 alarms=[d['api'] for d in manifest['optional'] if d['capability']=='alarm.service']
 if len(alarms)!=1 or alarms[0] not in (1,2) or manifest!=make_manifest(manifest['version'],alarms[0]):
  raise ValueError('Linked canonical declaration differs')
 return manifest

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--qualification',type=Path,required=True);p.add_argument('--background-object',type=Path,action='append',default=[]);p.add_argument('--version',required=True);p.add_argument('--alarm-api',type=int,choices=(1,2),required=True,help='Manifest API matching existing product authority; does not change the ELF');p.add_argument('--output',type=Path,required=True);p.add_argument('--cc');a=p.parse_args()
 q=a.qualification.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);cc=compiler_path(a.cc)
 receipt=json.loads((q/'qualification.json').read_text())
 for path,digest in receipt['dependency_sha256'].items():
  if hashlib.sha256(Path(path).read_bytes()).hexdigest()!=digest:raise ValueError('Qualification source changed: '+path)
 objects=[q/(name+'.o') for name in ('xtensa',*MODULES,'WebDavClient','WebDavRemoteVolume','PortablePeerFiles','sha2','PortableNetworkSession','PortableFileSetup','files_scene_background','UnsignedDivisionCompat','PortableTimeZone','PortableTimeZoneCatalog')]+[f.resolve() for f in a.background_object]
 # Four exact compiler support members are needed by GCC15 lowering. The
 # stock _udivdi3 crashes this linker; use the owning source compatibility
 # implementation already qualified by native application builds instead.
 libgcc=Path(subprocess.check_output([cc,'-print-libgcc-file-name'],text=True).strip())
 support=[]
 for name in ('_ashldi3.o','_lshrdi3.o','_bswapsi2.o','_bswapdi2.o'):
  member=out/name;member.write_bytes(subprocess.check_output([cc.removesuffix('gcc')+'ar','p',str(libgcc),name]));support.append(member)
 objects+=support
 exports={'app_main','risc_resident_app_descriptor_v1'};mapping=out/'exports.map';mapping.write_text('{global:'+ ';'.join(sorted(exports))+';local:*;};\n')
 elf=out/'file_browser.elf';cmd=[cc,'-shared','-nostdlib','-nostartfiles','-Wl,--no-relax','-Wl,--gc-sections','-Wl,--hash-style=sysv','-Wl,--version-script='+str(mapping),*map(str,objects),'-o',str(elf)]
 subprocess.run(cmd,check=True);linked=elf.read_bytes();subprocess.run(cmd,check=True);assert linked==elf.read_bytes(),'Non-identical repeated link'
 compaction=compact(elf,cc,out/'file_browser.full.elf');first=elf.read_bytes()
 repeat=out/'file_browser.repeat.elf';repeat.write_bytes(linked);compact(repeat,cc,out/'file_browser.repeat.full.elf')
 assert first==repeat.read_bytes(),'Non-identical repeated compaction'
 print(json.dumps({'canonical_bytes':len(first),'canonical_sha256':hashlib.sha256(first).hexdigest(),'compaction':compaction}),flush=True)
 syms=subprocess.check_output([cc.removesuffix('gcc')+'nm','-D',str(elf)],text=True);imports={s.split()[-1] for s in syms.splitlines() if ' U ' in ' '+s};actual={s.split()[-1] for s in syms.splitlines() if len(s.split())>=3 and s.split()[-2] in ('T','D','B','R')}
 allowed={'risc_runtime_get_api','memcpy','memset','memcmp','strcmp','strlen','strncmp','strrchr','snprintf','malloc','free','strcpy','memchr','strchr','strstr'}
 if not imports<=allowed or actual!=exports:raise ValueError('Unresolved/invalid application ABI: '+repr((imports-allowed,actual)))
 assert first[:7]==b'\x7fELF\x01\x01\x01' and first[16:20]==b'\x03\x00\x5e\x00'
 validator=out/'validate-elf';subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'),str(ROOT/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True);subprocess.run([str(validator),str(elf)],check=True)
 # Requirements describe optional discovery, never grant authority. Product
 # composition must preserve the exact original rows/export namespace maps.
 manifest=make_manifest(a.version,a.alarm_api)
 (out/'file_browser.json').write_text(json.dumps(manifest,indent=2)+'\n');(out/'link-receipt.json').write_text(json.dumps({'version':manifest['version'],'manifest_sha256':hashlib.sha256((out/'file_browser.json').read_bytes()).hexdigest(),'compaction':compaction,'bytes':len(first),'packaging_source_sha256':hashlib.sha256((ROOT/'scripts/compact_current_elf.py').read_bytes()).hexdigest(),'compiler_support_archive_sha256':hashlib.sha256(libgcc.read_bytes()).hexdigest(),'command':cmd,'sha256':hashlib.sha256(first).hexdigest(),'repeat_identical':True,'hardware_tested':False,'imports':sorted(imports),'exports':sorted(exports),'objects_sha256':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in objects},'qualification_sha256':hashlib.sha256((q/'qualification.json').read_bytes()).hexdigest(),'product_admission':'pending parent composition; no new grants authorized'},indent=2)+'\n')
if __name__=='__main__':main()

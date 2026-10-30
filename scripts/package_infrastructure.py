#!/usr/bin/env python3
"""Build and verify the complete offline ESP32-S3 first-install bundle."""
import argparse,hashlib,importlib.util,json,pathlib,shutil,subprocess,sys,zipfile
from infrastructure_fixtures import generate
from provision_infrastructure import seed
ROOT=pathlib.Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def commit(p):return subprocess.check_output(['git','-C',str(p),'rev-parse','HEAD'],text=True).strip()
def main():
 p=argparse.ArgumentParser();p.add_argument('--runtime',required=True,type=pathlib.Path);p.add_argument('--native-build',required=True,type=pathlib.Path);p.add_argument('--idf',required=True,type=pathlib.Path);p.add_argument('--cc',required=True);p.add_argument('--output',required=True,type=pathlib.Path);p.add_argument('--evidence',type=pathlib.Path);a=p.parse_args();r=a.runtime.resolve();b=a.native_build.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
 sys.path.insert(0,str(r/'scripts'));import paired_bank_images as paired;import spiffs_image
 subprocess.run([sys.executable,str(ROOT/'scripts/build_infrastructure.py'),'--runtime',str(r),'--cc',a.cc,'--output',str(out/'elf')],check=True)
 policy,state=generate(out/'fixtures',sha(out/'elf/infra-node.elf'));stage=out/'bootfs';shutil.copytree(out/'fixtures/bootfs',stage)
 for elf in (out/'elf').glob('*.elf'):shutil.copy2(elf,stage/elf.name)
 subprocess.run([sys.executable,str(a.idf/'components/spiffs/spiffsgen.py'),'0x510000',str(stage),str(out/'store.bin'),'--page-size','256','--block-size','4096','--obj-name-len','32','--meta-len','4','--use-magic','--use-magic-len'],check=True)
 extracted=spiffs_image.read_image((out/'store.bin').read_bytes(),0x510000);expected={p.name:p.read_bytes() for p in stage.iterdir()};assert extracted==expected,'SPIFFS extraction mismatch'
 for src,name in [(b/'riscrte_native.bin','firmware.bin'),(b/'bootloader/bootloader.bin','bootloader.bin'),(b/'partition_table/partition-table.bin','partitions.bin')]:shutil.copy2(src,out/name)
 fw=(out/'firmware.bin').read_bytes();store=(out/'store.bin').read_bytes();assert b'RISC_PAIRED_STORE_ABI:2\0' in fw and b'RISC_RUNTIME_VERSION:' in fw;assert 'CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y' in (b/'config/sdkconfig.h').read_text().replace('#define CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE 1','CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y')
 import re
 identities=set(re.findall(rb'RTE_SOURCE=([0-9a-f]{40})\x00',fw))
 if identities!={commit(r).encode()}:raise RuntimeError('Native binary source identity differs from Runtime HEAD; reconfigure and rebuild before packaging')
 (out/'otadata.bin').write_bytes(paired.initial_otadata());(out/'bank_state.bin').write_bytes(paired.initial_bank_state(fw,store,app_data=True));paired.parse_record((out/'bank_state.bin').read_bytes()[:96],app_data=True)
 seed(r,policy,state,out/'appdata-unprovisioned.bin')
 tools=out/'tools/littlefs';shutil.copytree(b/'esp-idf/risc_littlefs/pinned/littlefs',tools)
 scripts=out/'scripts';scripts.mkdir()
 for name in ['provision_infrastructure.py','infrastructure_seed.c','install_infrastructure.py','configure_infrastructure.mjs']:shutil.copy2(ROOT/'scripts'/name,scripts/name)
 if a.evidence:shutil.copytree(a.evidence,out/'evidence')
 docs=ROOT/'profiles/esp32s3-infrastructure';
 for name in ['README.md','CONFORMANCE.md','WORKSTATION-MAINTENANCE.md']:
  if (docs/name).exists():shutil.copy2(docs/name,out/name)
 offsets={'bootloader.bin':(0,0x8000),'partitions.bin':(0x8000,0x1000),'firmware.bin':(0x10000,0x260000),'appdata-unprovisioned.bin':(0x270000,0x80000),'store.bin':(0x2f0000,0x510000),'otadata.bin':(0xff0000,0x2000),'bank_state.bin':(0xff2000,0x2000)}
 files=[]
 for file in sorted(out.rglob('*')):
  if not file.is_file():continue
  rel=file.relative_to(out).as_posix();record={'path':rel,'size_bytes':file.stat().st_size,'sha256':sha(file)}
  if rel in offsets:record.update(offset=offsets[rel][0],partition_bytes=offsets[rel][1],first_install=True);assert record['size_bytes']<=record['partition_bytes']
  files.append(record)
 manifest={'schema':'esp32s3-infrastructure-install/1','profile':'esp32s3-infrastructure','chip':'esp32s3','flash_bytes':16777216,'flash_mode_runtime':'qio','flash_mode_writer':'dio','flash_frequency_mhz':80,'cpu_mhz':240,'psram':'octal 80MHz, required; actual capacity qualification pending','layout':'riscrte-paired-appdata-v2','layout_abi':2,'source':{'runtime':commit(r),'system_apps':commit(ROOT),'esp_idf':commit(a.idf),'spec':'f0e52f9fb704bd984a1b7ed8d0bdc9c74b2596a875d58ea74b7354f2c6667ac2'},'qualification':{'target_compiled':True,'hardware_qualified':'pending','cadence_qualified':'pending'},'omitted':{'nvs':'Preserve; no NVS image distributed','bank1':'Must be blank on first install; populated only by explicit later paired update','camera':'No camera driver or grant','ota_agent':'Not advertised; serial update only'},'files':files}
 (out/'flash-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');(out/'SHA256SUMS').write_text(''.join(x['sha256']+'  '+x['path']+'\n' for x in files)+sha(out/'flash-manifest.json')+'  flash-manifest.json\n')
 subprocess.run([sys.executable,str(out/'scripts/install_infrastructure.py'),'--bundle',str(out),'verify'],check=True)
 with zipfile.ZipFile(str(out)+'.zip','w',zipfile.ZIP_DEFLATED,compresslevel=9) as archive:
  for f in sorted(out.rglob('*')):
   if f.is_file():archive.write(f,out.name+'/'+f.relative_to(out).as_posix())
 print(json.dumps({'bundle':str(out)+'.zip','sha256':sha(pathlib.Path(str(out)+'.zip')),'size_bytes':pathlib.Path(str(out)+'.zip').stat().st_size},indent=2))
if __name__=='__main__':main()

#!/usr/bin/env python3
import hashlib,json,os,pathlib,shutil,subprocess,sys,zipfile
root=pathlib.Path(__file__).resolve().parents[1];build=root/'build';dist=root/'dist';dist.mkdir(exist_ok=True)
files={'bootloader/bootloader.bin':'bootloader.bin','partition_table/partition-table.bin':'partition-table.bin','biome_monitor.bin':'firmware.bin'}
for src,dst in files.items():shutil.copy2(build/src,dist/dst)
merged='biome-monitor-esp32c6-v1.1.1-0x0.bin'
subprocess.run([sys.executable,'-m','esptool','--chip','esp32c6','merge_bin','-o',str(dist/merged),'0x0',str(dist/'bootloader.bin'),'0x8000',str(dist/'partition-table.bin'),'0x30000',str(dist/'firmware.bin')],check=True)
# Verify exact placement and blank provisioning region in the delivery image.
image=(dist/merged).read_bytes()
for offset,name in [(0,'bootloader.bin'),(0x8000,'partition-table.bin'),(0x30000,'firmware.bin')]:
 data=(dist/name).read_bytes();assert image[offset:offset+len(data)]==data
assert image[0x9000:0x29000]==b'\xff'*0x20000
manifest={'name':'Biome Monitor','version':'1.1.1','target':'esp32c6','board':'Seengreat ESP32-C6 E-Paper Driver Board v1.4 (ESP32-C6FH4)','display':'UC8179 7.5 inch 800x480 B/W','idf':'5.4.2','source_revision':os.environ.get('GITHUB_SHA','local'),'pins':{'busy':2,'rst':6,'dc':3,'cs':7,'mosi':4,'sck':5},'hardware_tested':False,'flash_files':{'0x0':'bootloader.bin','0x8000':'partition-table.bin','0x30000':'firmware.bin'},'sha256':{name:hashlib.sha256((dist/name).read_bytes()).hexdigest() for name in files.values()}}
manifest['merged_image']={'file':merged,'offset':'0x0','provisioned':False,'sha256':hashlib.sha256(image).hexdigest()}
manifest['idle_logo']={'format':'mono1-msb-black1','width':800,'height':480,'size_bytes':48000,'sha256':hashlib.sha256((root/'main/assets/idle_logo.bin').read_bytes()).hexdigest()}
(dist/'BUILD_INFO.json').write_text(json.dumps(manifest,indent=2)+'\n');shutil.copy2(root/'README.md',dist/'README.md');shutil.copy2(root/'catalog.json',dist/'catalog.json')
with zipfile.ZipFile(dist/'source.zip','w',zipfile.ZIP_DEFLATED) as z:
 for p in sorted(root.rglob('*')):
  if p.is_file() and not any(x in ['build','dist','__pycache__','.git'] for x in p.relative_to(root).parts) and p.name!='sdkconfig' and not p.name.endswith('.sqlite3'):z.write(p,pathlib.Path('esp32c6-epaper')/p.relative_to(root))

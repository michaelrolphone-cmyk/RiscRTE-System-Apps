#!/usr/bin/env python3
import hashlib,json,os,pathlib,shutil,subprocess,sys,zipfile
root=pathlib.Path(__file__).resolve().parents[1];build=root/'build';dist=root/'dist';dist.mkdir(exist_ok=True)
files={'bootloader/bootloader.bin':'bootloader.bin','partition_table/partition-table.bin':'partition-table.bin','biome_monitor.bin':'firmware.bin'}
for src,dst in files.items():shutil.copy2(build/src,dist/dst)
# Check the linked artifact, including startup ordering, rather than merely
# claiming guards are enabled because their source files exist.
config=(build/'config/sdkconfig.h').read_text()
for name,value in [('ESP_INT_WDT',1),('ESP_INT_WDT_TIMEOUT_MS',300),('ESP_TASK_WDT_EN',1),('ESP_TASK_WDT_INIT',1),('ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0',1),('LWIP_SNTP_MAX_SERVERS',2)]:
 assert f'#define CONFIG_{name} {value}\n' in config,name
symbols={}
for line in subprocess.check_output(['riscv32-esp-elf-nm','-an',str(build/'biome_monitor.elf')],text=True).splitlines():
 fields=line.split()
 if len(fields)==3:symbols[fields[2]]=int(fields[0],16)
assert symbols['esp_system_init_fn_biome_radio_boot_reset']<symbols['esp_system_init_fn_init_heap']
assert 'biome_idle_guard_install' in symbols and 'keep_idle_responsive' in symbols
(dist/'runtime-empty.bin').write_bytes(b'\xff'*0x40000)
merged='biome-monitor-esp32c6-v1.1.3-0x0.bin'
subprocess.run([sys.executable,'-m','esptool','--chip','esp32c6','merge_bin','-o',str(dist/merged),'0x0',str(dist/'bootloader.bin'),'0x8000',str(dist/'partition-table.bin'),'0x30000',str(dist/'firmware.bin'),'0x200000',str(dist/'runtime-empty.bin')],check=True)
# Verify exact placement and blank provisioning region in the delivery image.
image=(dist/merged).read_bytes()
for offset,name in [(0,'bootloader.bin'),(0x8000,'partition-table.bin'),(0x30000,'firmware.bin')]:
 data=(dist/name).read_bytes();assert image[offset:offset+len(data)]==data
assert image[0x9000:0x29000]==b'\xff'*0x20000
assert image[0x200000:0x240000]==b'\xff'*0x40000
manifest={'name':'Biome','version':'1.1.3','target':'esp32c6','board':'Seengreat ESP32-C6 E-Paper Driver Board v1.4 (ESP32-C6FH4)','display':'UC8179 7.5 inch 800x480 B/W','idf':'5.4.2','source_revision':os.environ.get('GITHUB_SHA','local'),'pins':{'busy':2,'rst':6,'dc':3,'cs':7,'mosi':4,'sck':5},'hardware_tested':False,'flash_files':{'0x0':'bootloader.bin','0x8000':'partition-table.bin','0x30000':'firmware.bin','0x200000':'runtime-empty.bin'},'sha256':{name:hashlib.sha256((dist/name).read_bytes()).hexdigest() for name in [*files.values(),'runtime-empty.bin']}}
manifest['merged_image']={'file':merged,'offset':'0x0','provisioned':False,'sha256':hashlib.sha256(image).hexdigest()}
manifest['idle_logo']={'format':'mono1-msb-black1','width':800,'height':480,'size_bytes':48000,'sha256':hashlib.sha256((root/'main/assets/idle_logo.bin').read_bytes()).hexdigest()}
debug=dist/'debug';debug.mkdir(exist_ok=True)
for name in ['biome_monitor.elf','biome_monitor.map']:
 shutil.copy2(build/name,debug/name)
manifest['debug']={name:hashlib.sha256((debug/name).read_bytes()).hexdigest() for name in ['biome_monitor.elf','biome_monitor.map']}
manifest['recovery']={'interrupt_watchdog_enabled':True,'interrupt_watchdog_timeout_ms':300,'idle_wfi':False,'wifi_mac_reset_before_heap':True,'identity_restore_from_private_config':True,'idle_power_note':'CPU stays responsive instead of using WFI; idle power is higher.'}
manifest['recovery']['startup_table_radio_before_heap_verified']=True
manifest['recovery'].update(journal_partition='runtime',journal_offset='0x200000',journal_bytes=0x40000,alternating_crc_snapshots=True,exact_intent_readback=True,panel_retry_seconds=60,http_timeout_seconds=45,initial_time_wait_seconds=30,automatic_provisioning_erase=False)
assert 'runtime,data,nvs,0x200000,0x40000' in (root/'partitions.csv').read_text()
(dist/'BUILD_INFO.json').write_text(json.dumps(manifest,indent=2)+'\n');shutil.copy2(root/'README.md',dist/'README.md');shutil.copy2(root/'catalog.json',dist/'catalog.json')
with zipfile.ZipFile(dist/'source.zip','w',zipfile.ZIP_DEFLATED) as z:
 for p in sorted(root.rglob('*')):
  if p.is_file() and not any(x in ['build','dist','__pycache__','.git'] for x in p.relative_to(root).parts) and p.name not in ['sdkconfig','sdkconfig.old'] and not p.name.endswith('.sqlite3'):z.write(p,pathlib.Path('esp32c6-epaper')/p.relative_to(root))

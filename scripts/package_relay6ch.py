#!/usr/bin/env python3
import argparse,hashlib,json,pathlib,shutil,subprocess,zipfile
ROOT=pathlib.Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def head(p):return subprocess.check_output(['git','-C',str(p),'rev-parse','HEAD'],text=True).strip()
def main():
 p=argparse.ArgumentParser();p.add_argument('--build',type=pathlib.Path,required=True);p.add_argument('--runtime',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args();out=a.output;out.mkdir(exist_ok=True,parents=True)
 image=bytearray(b'\xff'*0x800000);components=[]
 for name,path,offset,limit in [('bootloader.bin','bootloader/bootloader.bin',0,0x8000),('partitions.bin','partition_table/partition-table.bin',0x8000,0x1000),('riscrte-relay6ch-v1.0.0-application.bin','riscrte_relay6ch.bin',0x50000,0x300000)]:
  data=(a.build/path).read_bytes();assert 0<len(data)<=limit
  if 'partitions' not in name:assert data[0]==0xe9 and int.from_bytes(data[12:14],'little')==9,'Expected ESP32-S3 image'
  (out/name).write_bytes(data);image[offset:offset+len(data)]=data;components.append({'asset':name,'offset':offset,'size_bytes':len(data),'sha256':sha(out/name)})
 (out/'riscrte-relay6ch-v1.0.0-initial.bin').write_bytes(image)
 shutil.copy2(ROOT/'profiles/waveshare-esp32-s3-relay-6ch/README.md',out/'README.md');shutil.copy2(ROOT/'profiles/waveshare-esp32-s3-relay-6ch/catalog.json',out/'catalog.json')
 shutil.copy2(ROOT/'LICENSE',out/'SYSTEM-APPS-LICENSE');shutil.copy2(a.runtime/'LICENSE',out/'RUNTIME-LICENSE')
 manifest={'profile':'waveshare-esp32-s3-relay-6ch','version':'1.0.0','chip':'esp32s3','flash_bytes':len(image),'requires_psram':False,'flash_mode':'dio','flash_frequency_mhz':40,'initial_image':{'asset':'riscrte-relay6ch-v1.0.0-initial.bin','offset':0,'first_install_only':True,'replaces_saved_data':True,'sha256':sha(out/'riscrte-relay6ch-v1.0.0-initial.bin')},'components':components,'sources':{'apps':head(ROOT),'runtime':head(a.runtime)},'qualification':{'target_compiled':True,'hardware_tested':False}}
 (out/'flash-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
 with zipfile.ZipFile(out/'riscrte-relay6ch-v1.0.0-install.zip','w',zipfile.ZIP_DEFLATED) as z:
  for f in sorted(out.iterdir()):
   if f.is_file() and f.suffix!='.zip' and f.name!='SHA256SUMS':z.write(f,f.name)
 (out/'SHA256SUMS').write_text(''.join(sha(f)+'  '+f.name+'\n' for f in sorted(out.iterdir()) if f.is_file() and f.name!='SHA256SUMS'))
if __name__=='__main__':main()

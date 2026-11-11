#!/usr/bin/env python3
"""Verify, preflight, first-install, or update this exact paired bank profile.
Serial modes require explicit invocation. Routine update never writes app-data,
NVS, partition table or bootloader. All device reads are private backups."""
import argparse,hashlib,json,os,pathlib,re,struct,subprocess,sys,time,zlib
if not __debug__:
 raise RuntimeError('Installer safety validation requires Python assertions; do not run with -O')
FLASH=0x1000000;FW=[0x10000,0x800000];STORE=[0x2f0000,0xae0000];DATA=(0x270000,0x80000)
def digest(data):return hashlib.sha256(data).hexdigest()
def verify(bundle):
 m=json.loads((bundle/'flash-manifest.json').read_text());assert m['schema']=='esp32s3-infrastructure-install/1' and m['layout_abi']==2 and m['chip']=='esp32s3' and m['flash_bytes']==FLASH
 ranges=[]
 for f in m['files']:
  path=bundle/f['path'];assert path.resolve().is_relative_to(bundle.resolve()) and not path.is_symlink();data=path.read_bytes();assert len(data)==f['size_bytes'] and digest(data)==f['sha256'],f['path']
  if 'offset'in f:
   start,end=f['offset'],f['offset']+f['partition_bytes'];assert start%4096==0 and 0<=start<end<=FLASH and len(data)<=f['partition_bytes'];assert not any(start<b and a<end for a,b in ranges);ranges.append((start,end))
 assert b'RISC_PAIRED_STORE_ABI:2\0' in (bundle/'firmware.bin').read_bytes();return m
def record(bank,fw,store):
 b=struct.pack('<6I32s32sI',0x314b4252,1,bank,len(fw),len(store),2,hashlib.sha256(fw).digest(),hashlib.sha256(store).digest(),0);return b+struct.pack('<I',zlib.crc32(b))
def parse_record(data,bank):
 assert len(data)>=96 and zlib.crc32(data[:92])==struct.unpack_from('<I',data,92)[0];v=struct.unpack('<6I32s32sII',data[:96]);assert v[:3]==(0x314b4252,1,bank) and 32<=v[3]<=0x260000 and v[4:6]==(0x510000,2) and not v[8];return v
def active_bank(flash):
 entries=[]
 for i in range(2):
  x=flash[0xff0000+i*4096:0xff0000+i*4096+32];seq,state,crc=struct.unpack_from('<I',x)[0],struct.unpack_from('<I',x,24)[0],struct.unpack_from('<I',x,28)[0]
  if seq in (0,0xffffffff) or zlib.crc32(x[:4],0xffffffff)!=crc or state in (3,4):continue
  assert state in (0,1,2),'Unknown OTA state'
  entries.append((seq,(seq-1)%2,i,state))
 assert entries,'No valid bank; use explicit recovery with a verified bank'
 assert len(entries)<2 or entries[0][0]!=entries[1][0],'Ambiguous equal OTA sequences'
 selected=max(entries)
 assert selected[3]==2,'Newest selected bank is not confirmed; inspect boot/rollback before updating'
 return selected[:3]
def update_plan(bundle,flash,recovery_bank=None):
 assert len(flash)==FLASH;assert flash[0x8000:0x8000+3072]==(bundle/'partitions.bin').read_bytes(),'Partition layout mismatch'
 loader=(bundle/'bootloader.bin').read_bytes()
 # Published v1.4.2 differs only in build timestamp and its checksum/digest.
 # Preserve that exact bootloader; no bootloader write is in an update plan.
 assert flash[:len(loader)]==loader or (len(loader)==22640 and digest(flash[:22640])=='6ef091e6fa1c102ff4449b47677d52fb13ea4078b1888b48bb0a0ed8c24bc3fb'),'Bootloader differs; explicit compatibility/migration required'
 if recovery_bank is None:sequence,active,ota_slot=active_bank(flash);target=1-active
 else:
  active=recovery_bank;sequence=max([struct.unpack_from('<I',flash,0xff0000+i*4096)[0] for i in range(2) if struct.unpack_from('<I',flash,0xff0000+i*4096)[0]!=0xffffffff]or[0]);target=active;ota_slot=0
 current=parse_record(flash[0xff2000+active*4096:],active);assert digest(flash[FW[active]:FW[active]+current[3]])==current[6].hex() and digest(flash[STORE[active]:STORE[active]+current[4]])==current[7].hex(),'Active pair readback mismatch'
 if recovery_bank is None:
  fw=(bundle/'firmware.bin').read_bytes();store=(bundle/'store.bin').read_bytes();writes=[(FW[target],fw),(STORE[target],store),(0xff2000+target*4096,record(target,fw,store)+b'\xff'*(4096-96))]
 else:writes=[]
 next_seq=sequence+1
 while (next_seq-1)%2!=target:next_seq+=1
 assert next_seq<0xfffffffe
 seq=struct.pack('<I',next_seq);ota=seq+b'\xff'*20+struct.pack('<II',0 if recovery_bank is None else 2,zlib.crc32(seq,0xffffffff));writes.append((0xff0000+(1-ota_slot)*4096,ota+b'\xff'*(4096-32)))
 for offset,data in writes:assert not(offset<DATA[0]+DATA[1] and DATA[0]<offset+len(data));assert offset>=0x10000
 return writes
class Serial:
 def __init__(self,port,baud):self.base=[sys.executable,'-m','esptool','--chip','esp32s3','--port',port,'--baud',str(baud),'--after','no-reset']
 def call(self,*args):return subprocess.check_output(self.base+list(map(str,args)),text=True,stderr=subprocess.STDOUT)
 def check(self):
  chip=self.call('chip-id');assert 'ESP32-S3' in chip or 'ESP32S3' in chip,'Wrong chip'
  size=self.call('flash-id');assert re.search(r'Detected flash size:\s*16\s*MB',size,re.I),'Requires verified16MiB flash'
  security=self.call('get-security-info');assert re.search(r'Secure Boot:\s*Disabled',security,re.I) and re.search(r'Flash Encryption:\s*Disabled',security,re.I),'This serial flow requires disabled secure boot/encryption'
  return chip+'\n'+size+'\n'+security
 def write(self,offset,path):print(self.call('write-flash','--flash-mode','dio','--flash-size','16MB','--flash-freq','80m',hex(offset),path));print(self.call('verify-flash',hex(offset),path))
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--bundle',type=pathlib.Path,default=pathlib.Path(__file__).resolve().parents[1]);p.add_argument('--port');p.add_argument('--baud',type=int,default=460800);p.add_argument('--backup',type=pathlib.Path);p.add_argument('--appdata',type=pathlib.Path);p.add_argument('--bank',type=int,choices=[0,1]);p.add_argument('mode',choices=['verify','preflight','first-install','update','recover']);a=p.parse_args();m=verify(a.bundle);print('Bundle hashes, ranges and ABI verified.')
 if a.mode=='verify':return
 if not a.port or not a.backup:p.error('Serial modes require --port and a new private --backup directory')
 a.backup.mkdir(parents=True,exist_ok=False);os.chmod(a.backup,0o700);serial=Serial(a.port,a.baud);(a.backup/'preflight.txt').write_text(serial.check());backup=a.backup/'flash-before.bin';print(serial.call('read-flash','0',hex(FLASH),backup));os.chmod(backup,0o600);flash=backup.read_bytes();assert len(flash)==FLASH;(a.backup/'backup-sha256.txt').write_text(digest(flash)+'\n')
 if a.mode=='preflight':print('Preflight and private16MiB backup complete. No device writes.');return
 if a.mode=='first-install':
  table=flash[0x8000:0x9000];assert all(x==255 for x in table) or table[:3072]==(a.bundle/'partitions.bin').read_bytes(),'Existing incompatible partition table'
  assert all(x==255 for x in flash[DATA[0]:DATA[0]+DATA[1]]),'Existing app-data must be preserved; use update/recover'
  assert all(x==255 for x in flash[FW[1]:FW[1]+0x260000]) and all(x==255 for x in flash[STORE[1]:STORE[1]+0x510000]),'Inactive bank must be blank for first-install'
  writes=[]
  for f in m['files']:
   if 'offset' not in f:continue
   path=a.appdata if f['path']=='appdata-unprovisioned.bin' and a.appdata else a.bundle/f['path'];data=path.read_bytes();assert len(data)<=f['partition_bytes'];writes.append((f['offset'],data))
  # Readiness and activation metadata are written only after both images verify.
  writes.sort(key=lambda x:(x[0]>=0xff0000,x[0]==0xff0000,x[0]))
 else:
  if a.appdata:p.error('update/recover never accepts an app-data replacement')
  if a.mode=='recover' and a.bank is None:p.error('recover requires explicit --bank0 or1')
  writes=update_plan(a.bundle,flash,a.bank if a.mode=='recover' else None)
 for i,(offset,data) in enumerate(writes):path=a.backup/f'write-{i}.bin';path.write_bytes(data);os.chmod(path,0o600);serial.write(offset,path)
 print('All writes verified. Reset the board. Preserve the backup; it contains private data.')
if __name__=='__main__':main()

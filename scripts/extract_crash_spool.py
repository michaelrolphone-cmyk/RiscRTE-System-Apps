#!/usr/bin/env python3
"""Extract checked report text from a retrieved AppData crash-spool.bin (no device I/O)."""
import argparse,struct,zlib
from pathlib import Path

def extract(raw):
 if len(raw)<80 or raw[:8]!=b'CRSHSPL\0':raise ValueError('Not a crash spool')
 version,header,size,count=struct.unpack_from('<HHII',raw,8)
 if version!=1 or header!=80 or size!=len(raw) or count>8 or len(raw)>33232:raise ValueError('Invalid spool geometry')
 expected=struct.unpack_from('<I',raw,76)[0]
 if zlib.crc32(raw[:76]+b'\0'*4+raw[80:])!=expected:raise ValueError('Spool CRC mismatch')
 if any(raw[64:76]):raise ValueError('Nonzero reserved header')
 at=80;reports=[]
 for _ in range(count):
  if at+48>len(raw):raise ValueError('Truncated entry')
  sha=raw[at:at+32].hex();boot,sequence,n,reserved=struct.unpack_from('<IIII',raw,at+32);at+=48
  if reserved or not 0<n<=4096 or at+n>len(raw):raise ValueError('Invalid entry')
  text=raw[at:at+n];text.decode('utf-8');at+=n
  if any(x<32 and x not in (9,10,13) or x==127 for x in text):raise ValueError('Invalid report text')
  reports.append((f'{sha}-{boot:08x}-{sequence:08x}-{zlib.crc32(text):08x}.txt',text))
 if at!=len(raw):raise ValueError('Trailing bytes')
 return reports

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('spool',type=Path);p.add_argument('output',type=Path);a=p.parse_args();reports=extract(a.spool.read_bytes());a.output.mkdir(parents=True,exist_ok=True)
 for name,text in reports:
  out=a.output/name
  if out.exists() and out.read_bytes()!=text:raise ValueError('Existing report differs: '+name)
  out.write_bytes(text);print(out)
if __name__=='__main__':main()

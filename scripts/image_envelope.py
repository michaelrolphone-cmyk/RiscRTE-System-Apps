"""RTEIMG1 ordinary-application storage envelope; canonical ELF bytes are unchanged.
Format matches owning Runtime ImageEnvelope v1: 32-byte LE header, raw LZ4 block,
CRC32 of canonical output. CRC detects corruption; it is NOT authentication.
Native store/cohort authority and loader validation remain mandatory.
"""
import hashlib,struct,zlib
MAGIC=b'RTEIMG1\0'
HEADER=struct.Struct('<8s6I')
MAX_IMAGE=2*1024*1024
def lz4_block(data):
 out=bytearray();table={};anchor=pos=0;n=len(data)
 def length(value):
  while value>=255:out.append(255);value-=255
  out.append(value)
 while pos+12<=n:
  key=data[pos:pos+4];previous=table.get(key);table[key]=pos
  if previous is None or pos-previous>65535:
   pos+=1;continue
  end=pos+4
  # Preserve five terminal literals and the standard last-match constraint.
  while end<n-5 and data[previous+end-pos]==data[end]:end+=1
  literals=pos-anchor;match=end-pos-4
  out.append((min(literals,15)<<4)|min(match,15))
  if literals>=15:length(literals-15)
  out.extend(data[anchor:pos]);out.extend((pos-previous).to_bytes(2,'little'))
  if match>=15:length(match-15)
  pos=end;anchor=pos
 tail=n-anchor;out.append(min(tail,15)<<4)
 if tail>=15:length(tail-15)
 out.extend(data[anchor:]);return bytes(out)
def decode(block,size):
 out=bytearray();pos=0
 def length(base):
  nonlocal pos
  if base==15:
   while True:
    if pos>=len(block):raise ValueError('Truncated length')
    value=block[pos];pos+=1;base+=value
    if value!=255:break
  return base
 while pos<len(block):
  token=block[pos];pos+=1;count=length(token>>4)
  if count>len(block)-pos or count>size-len(out):raise ValueError('Literal bounds')
  out.extend(block[pos:pos+count]);pos+=count
  if pos==len(block):break
  if pos+2>len(block):raise ValueError('Truncated offset')
  distance=int.from_bytes(block[pos:pos+2],'little');pos+=2
  if not distance or distance>len(out):raise ValueError('Invalid distance')
  count=length(token&15)+4
  if count>size-len(out):raise ValueError('Match bounds')
  for _ in range(count):out.append(out[-distance])
 if len(out)!=size:raise ValueError('Output length differs')
 return bytes(out)

def encode_image(raw):
 if not isinstance(raw,bytes) or len(raw)<52 or len(raw)>MAX_IMAGE or raw[:4]!=b'\x7fELF':
  raise ValueError('Expected bounded canonical ELF')
 payload=lz4_block(raw)
 encoded=HEADER.pack(MAGIC,HEADER.size,1,len(raw),len(payload),zlib.crc32(raw),0)+payload
 if decode_image(encoded)!=raw:raise ValueError('Encoder roundtrip differs')
 return encoded
def decode_image(encoded,maximum=MAX_IMAGE):
 if len(encoded)<HEADER.size:raise ValueError('Truncated image envelope')
 magic,header,codec,logical,stored,crc,reserved=HEADER.unpack_from(encoded)
 if magic!=MAGIC or header!=HEADER.size or codec!=1 or reserved:raise ValueError('Unsupported image envelope')
 if logical<52 or logical>maximum or not stored or stored!=len(encoded)-HEADER.size:raise ValueError('Image envelope bounds')
 raw=decode(encoded[HEADER.size:],logical)
 if raw[:4]!=b'\x7fELF' or zlib.crc32(raw)!=crc:raise ValueError('Canonical image corruption')
 return raw
def receipt(raw,encoded):
 if decode_image(encoded)!=raw:raise ValueError('Canonical image mismatch')
 return {'format':'RTEIMG1','header_bytes':HEADER.size,'codec':'lz4-block',
         'canonical_bytes':len(raw),'canonical_sha256':hashlib.sha256(raw).hexdigest(),
         'stored_bytes':len(encoded),'stored_sha256':hashlib.sha256(encoded).hexdigest(),
         'crc32':zlib.crc32(raw),'roundtrip_verified':True}

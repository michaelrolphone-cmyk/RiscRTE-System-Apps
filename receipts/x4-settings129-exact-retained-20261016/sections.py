import struct,pathlib,sys
class ELF:
 def __init__(self,p):
  self.b=bytearray(pathlib.Path(p).read_bytes()); h=struct.unpack_from('<16sHHIIIIIHHHHHH',self.b); off,sz,n,st=h[6],h[11],h[12],h[13]; self.sh=[struct.unpack_from('<IIIIIIIIII',self.b,off+i*sz) for i in range(n)];ss=self.sh[st]; strs=self.b[ss[4]:ss[4]+ss[5]];self.sections={bytes(strs[s[0]:]).split(b'\0')[0].decode():s for s in self.sh}
 def get(self,n):
  s=self.sections[n];return self.b[s[4]:s[4]+s[5]]
 def text_norm(self):
  b=self.get('.text');s=self.sections['.text']
  for rel in ['.rela.dyn','.rela.plt']:
   x=self.get(rel)
   for j in range(0,len(x),12):
    at=struct.unpack_from('<I',x,j)[0]-s[3]
    if 0<=at<len(b):b[at:at+4]=b'\0'*4
  return b
if __name__=='__main__':
 a,z=ELF(sys.argv[1]),ELF(sys.argv[2]);print('sections');
 for n in a.sections:
  if n and a.sections[n][1]!=8:
   x,y=a.get(n),z.get(n);print(n,len(x),len(y),'same'if x==y else 'different')
 x,y=a.text_norm(),z.text_norm(); diff=[i for i in range(min(len(x),len(y))) if x[i]!=y[i]];print('normalized text bytes differ',len(diff),'first',hex(a.sections['.text'][3]+diff[0])if diff else'none')

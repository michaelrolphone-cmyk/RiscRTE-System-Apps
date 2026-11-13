from pathlib import Path
import subprocess,os,json
import argparse
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[1];o=a.output;o.mkdir(parents=True,exist_ok=True)
p='lib/PortableApps/src/nova.inc';old=subprocess.check_output(['git','-C',r,'show','83da63b:'+p],text=True);new=(r/p).read_text()
def f(s,name):
 a=s.index('static '+('unsigned' if name=='np_root' else 'void')+' '+name+'(');b=s.index('{',a)+1;level=1
 while level:level+=(s[b]=='{')-(s[b]=='}');b+=1
 return s[a:b]
pre='''#include <stdint.h>\n#include <stdbool.h>\n#include <assert.h>\n#include <stdio.h>\n#define PORTABLE_RASTER_SNAPSHOT\nstatic int bandtop,bandbottom;static unsigned calls,expected_calls,roots;static bool compare;struct px{int x,y;unsigned a;};static struct px pixels[100000];\nstatic void raster_rows(int64_t origin,int*f,int*e){if(origin+*f<bandtop)*f=bandtop-origin;if(origin+*e>bandbottom)*e=bandbottom-origin;}\nstatic void np_pixel(int x,int y,uint32_t rgb,unsigned a){assert(rgb==0);if(compare){assert(calls<expected_calls);assert(pixels[calls].x==x&&pixels[calls].y==y&&pixels[calls].a==a);}else pixels[calls]=(struct px){x,y,a};calls++;}\n'''
s=pre+f(new,'np_root').replace('{','{++roots;',1)+f(old,'np_caption_backing').replace('np_caption_backing(','prior(',1)+f(new,'np_caption_backing')+'''
int main(void){unsigned tests=0;for(int w=1;w<=240;w+=7)for(int h=1;h<=60;h+=3)for(int band=0;band<2;band++){bandtop=band?41:-1000;bandbottom=band?54:1000;calls=roots=0;compare=false;prior(11,30,11+w,30+h);expected_calls=calls;unsigned oldroots=roots;calls=roots=0;compare=true;np_caption_backing(11,30,11+w,30+h);assert(calls==expected_calls);assert(roots<=oldroots);tests++;}bandtop=-1000;bandbottom=1000;calls=roots=0;compare=false;prior(0,0,200,24);unsigned before=roots;expected_calls=calls;calls=roots=0;compare=true;np_caption_backing(0,0,200,24);printf("PASS %u complete caption pixel/call-order cases;200x24 root calls %u -> %u\\n",tests,before,roots);}
'''
(o/'caption.c').write_text(s);rows=[]
for san in (False,True):
 exe=o/'caption';cmd=['cc','-O2','-Wall','-Wextra','-Werror',str(o/'caption.c'),'-o',str(exe)]
 if san:cmd+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie']
 subprocess.run(cmd,check=True);v=subprocess.check_output([str(exe)],text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});print(v);rows.append({'command':cmd,'result':v});exe.unlink()
(o/'caption.json').write_text(json.dumps(rows,indent=2))

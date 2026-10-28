from pathlib import Path
import argparse,subprocess,json,os
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();r=Path(__file__).resolve().parents[1];o=a.output;o.mkdir(parents=True,exist_ok=True)
def fn(s,name):
 start=s.index('static void '+name+'(')
 if name=='nu_pixel':return s[start:s.index('static void nu_rect(',start)]
 end=s.index('{',start)+1;depth=1
 while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[start:end]
s='''#include <stdint.h>\n#include <stdbool.h>\n#include <stddef.h>\n#include <assert.h>\n#include <stdio.h>\n#include <string.h>\n#include "PortableRgb565Blend.h"\n#define PORTABLE_RASTER_SNAPSHOT\n#define RISC_DISPLAY_FORMAT_MONO1 1\nstatic bool failed,raster_replaying;static unsigned raster_pixels,checks,stop;static int raster_band_top=30,raster_band_bottom=200;static bool raster_band_columns;static unsigned surface_format=2;static uint8_t pixels[466*466*2],expected[sizeof pixels];static struct{void*pixels;size_t stride_bytes;}surface={pixels,932};enum{RS_NP_PIXEL,RS_NU_PIXEL};\nstatic bool raster_record(int op,const int32_t*a,const char*s){(void)op;(void)a;(void)s;return false;}static bool raster_checkpoint(void){++checks;if(stop&&checks==stop){failed=true;return false;}return !failed;}static bool raster_surface_writable(void){return !failed;}static int width(void){return 466;}static int height(void){return 466;}static int nu_height(void){return 240;}static void native_point(int*x,int*y){(void)x;(void)y;}static int nu_native_band_offset(bool b){(void)b;return 0;}static uint16_t np_rgb(uint32_t c){return portable_rgb565_pack(c);}\n'''
for file,name in [('nova.inc','np_pixel'),('nova_native.inc','nu_native_pixel'),('nova_ui.inc','nu_pixel')]:
 path='lib/PortableApps/src/'+file;old=subprocess.check_output(['git','-C',r,'show','83da63b:'+path],text=True);new=(r/path).read_text();s+=fn(old,name).replace(name+'(','old_'+name+'(',1)+'\n'+fn(new,name)+'\n'
s+='''int main(void){unsigned cases=0;uint32_t seed=1;void(*old[])(int,int,uint32_t,unsigned)={old_np_pixel,old_nu_native_pixel,old_nu_pixel};void(*now[])(int,int,uint32_t,unsigned)={np_pixel,nu_native_pixel,nu_pixel};for(unsigned f=0;f<3;++f)for(unsigned n=0;n<4000;++n){seed=seed*1664525+1013904223;int x=(int)(seed%500)-10;seed=seed*1664525+1013904223;int y=(int)(seed%500)-10;unsigned alpha=seed%512;uint32_t color=seed>>3;raster_replaying=n&1;stop=n%5?0:1;memset(pixels,0x3e,sizeof pixels);failed=false;raster_pixels=511;checks=0;old[f](x,y,color,alpha);unsigned c=checks,p=raster_pixels;bool fail=failed;memcpy(expected,pixels,sizeof pixels);memset(pixels,0x3e,sizeof pixels);failed=false;raster_pixels=511;checks=0;now[f](x,y,color,alpha);assert(c==checks&&p==raster_pixels&&fail==failed&&!memcmp(expected,pixels,sizeof pixels));cases++;}printf("PASS %u actual pixel caller parity, band and failure-boundary cases\\n",cases);}\n'''
(o/'pixel.c').write_text(s);rows=[]
for san in (False,True):
 exe=o/'pixel';cmd=['cc','-O2','-Wall','-Wextra','-Werror','-I'+str(r/'lib/PortableApps/include'),str(o/'pixel.c'),'-o',str(exe)]
 if san:cmd+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie']
 subprocess.run(cmd,check=True);v=subprocess.check_output([str(exe)],text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});print(v);rows.append({'command':cmd,'result':v});exe.unlink()
(o/'result.json').write_text(json.dumps(rows,indent=2))

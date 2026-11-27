#!/usr/bin/env python3
"""Exact original mask oracle across affine geometries and interruption rows."""
from pathlib import Path
import subprocess,json,os,argparse,hashlib
r=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();o=a.output.resolve();o.mkdir(parents=True,exist_ok=True)
def function(s,name):
 start=s.index('static void '+name+'(');end=s.index('{',start)+1;level=1
 while level:level+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[start:end]
old=subprocess.check_output(['git','-C',str(r),'show','fb868fe:lib/PortableApps/src/nova.inc'],text=True);new=(r/'lib/PortableApps/src/nova.inc').read_text();adapter=(r/'lib/PortableApps/src/adapter.c').read_text()
pre=r'''#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define PORTABLE_RASTER_SNAPSHOT
#define PORTABLE_SAFE_VIEWPORT
#define PORTABLE_PAPER_PREFERENCES
#define RS_NP_MASK 0
static int W,H;static bool paper_rotated,paper_flip_ui,nova_mask_pending,failed,writable,raster_replaying;
static unsigned callbacks,fail_after;static int raster_band_top,raster_band_bottom;
static struct {unsigned height;} info;
static struct {unsigned x,y;} content_view;
static struct {void*pixels;size_t stride_bytes;} surface;
static int width(void){return W;}static int height(void){return H;}
static bool raster_record(unsigned k,const int32_t*a,const char*s){(void)k;(void)a;(void)s;assert(!failed);return false;}
static bool raster_surface_writable(void){return writable;}
static bool raster_checkpoint(void){assert(!failed);++callbacks;if(fail_after&&callbacks==fail_after){failed=true;writable=false;return false;}return true;}
static void raster_rows(int64_t origin,int*f,int*e){if(!raster_replaying)return;int64_t top=raster_band_top-origin,bottom=raster_band_bottom-origin;if(top>*f)*f=top<*e?(int)top:*e;if(bottom<*e)*e=bottom>*f?(int)bottom:*f;}
'''
main=r'''
static unsigned cases;
static void run(int w,int h,unsigned profile,unsigned pattern,unsigned band,unsigned stop){
 W=w;H=h;paper_rotated=profile==1||profile==3;paper_flip_ui=profile==2||profile==3;
 content_view.x=profile==4?13:0;content_view.y=profile==4?17:0;
 unsigned physical_w=paper_rotated?h:w,physical_h=paper_rotated?w:h;
 if(profile==4){physical_w+=31;physical_h+=37;}info.height=physical_h;
 surface.stride_bytes=physical_w*2+10;size_t bytes=64+surface.stride_bytes*physical_h+64;
 unsigned char*initial=malloc(bytes),*expected=malloc(bytes),*actual=malloc(bytes);assert(initial&&expected&&actual);memset(initial,0xa5,bytes);
 for(unsigned y=0;y<physical_h;y++)for(unsigned x=0;x<physical_w;x++){
  uint16_t v=pattern==0?0:pattern==1?0xffff:(uint16_t)((x*173u+y*2357u)^0x9753u);
  if(pattern==3&&(x+y)%7)v=0;
  initial[64+y*surface.stride_bytes+x*2]=(uint8_t)v;initial[65+y*surface.stride_bytes+x*2]=(uint8_t)(v>>8);
 }
 raster_replaying=band!=0;raster_band_top=band==2?H-3:10;raster_band_bottom=band==2?H:17;
 memcpy(expected,initial,bytes);surface.pixels=expected+64;nova_mask_pending=true;failed=false;writable=true;callbacks=0;fail_after=stop;np_mask_reference();unsigned oldcalls=callbacks;bool oldfailed=failed;
 memcpy(actual,initial,bytes);surface.pixels=actual+64;nova_mask_pending=true;failed=false;writable=true;callbacks=0;np_mask();
 assert(callbacks==oldcalls&&failed==oldfailed);assert(!memcmp(expected,actual,bytes));cases++;
 if(failed){unsigned n=callbacks;nova_mask_pending=false;np_mask();assert(callbacks==n);assert(!memcmp(expected,actual,bytes));}
 free(actual);free(expected);free(initial);
}
int main(void){const int sizes[][2]={{160,160},{240,240},{300,330},{466,466},{480,800},{800,480},{1024,1024}};
 for(unsigned s=0;s<7;s++)for(unsigned p=0;p<5;p++)for(unsigned pattern=0;pattern<4;pattern++)for(unsigned band=0;band<3;band++)run(sizes[s][0],sizes[s][1],p,pattern,band,0);
 for(unsigned p=0;p<5;p++)for(unsigned stop=1;stop<=8;stop++)run(466,466,p,2,0,stop);
 printf("PASS %u complete-buffer mask geometry/background/band/fault comparisons; exact checkpoint sequence\n",cases);}
'''
s=pre+function(adapter,'native_point')+function(old,'np_mask').replace('np_mask(','np_mask_reference(',1)+function(new,'np_mask_region')+function(new,'np_mask')+main
(o/'mask.c').write_text(s);receipts=[]
for san in [False,True]:
 cmd=['cc','-std=c11','-O2','-Wall','-Wextra','-Werror',str(o/'mask.c'),'-o',str(o/'mask')]
 if san:cmd+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie']
 subprocess.run(cmd,check=True);z=subprocess.check_output([str(o/'mask')],text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'});print(z,end='');receipts.append(dict(command=cmd,result=z,sanitized=san))
(o/'results.json').write_text(json.dumps(dict(source_sha256=hashlib.sha256(new.encode()).hexdigest(),receipts=receipts),indent=2)+'\n')

from pathlib import Path
import subprocess,json,os
import argparse
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[1];o=a.output;o.mkdir(parents=True,exist_ok=True)
s=(r/'lib/PortableApps/src/settings_view.inc').read_text();f=s[s.index('static bool sv_native_rect('):s.index('static void sv_rect(')]
preamble=r'''
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "PortableRgb565Blend.h"
#define PORTABLE_RASTER_SNAPSHOT
#define RISC_DISPLAY_FORMAT_RGB565 2
static unsigned surface_format=2,raster_pixels,calls,fail_at,invalidate_at,transforms,scales;
static bool failed,raster_replaying,flip,rotate,unbound;
static int raster_band_top,raster_band_bottom,side,inset,physical;
static uint8_t actual[520*520*2],reference[sizeof(actual)];
static struct {void*pixels;size_t stride_bytes;}surface;
static bool raster_surface_writable(void){return !failed&&!unbound;}
static bool raster_checkpoint(void){++calls;if(invalidate_at&&calls==invalidate_at){memcpy(reference,actual,sizeof(actual));unbound=true;return true;}if(fail_at&&calls==fail_at){failed=true;return false;}return !failed;}
static int edge(int n){++scales;int v=n*side;return v>=0?v/240:-((-v+239)/240);}
static int sv_native_x(int n){return edge(n);}
static int sv_native_y(int n){return edge(n);}
static void native_point(int*x,int*y){++transforms;if(flip){*x=side-1-*x;*y=side-1-*y;}if(rotate){int t=*x;*x=*y;*y=side-1-t;}*x+=inset;*y+=inset;}
static void old_rect(int x,int y,int w,int h,uint32_t rgb){
for(int j=y;j<y+h;++j)for(int i=x;i<x+w;++i){
 if(i<0||i>=240||j<0||j>=240)continue;
 for(int yy=sv_native_y(j),ye=sv_native_y(j+1);yy<ye;++yy)for(int xx=sv_native_x(i),xe=sv_native_x(i+1);xx<xe;++xx){
 if(raster_replaying&&(yy<raster_band_top||yy>=raster_band_bottom))continue;
 int px=xx,py=yy;native_point(&px,&py);uint16_t c=portable_rgb565_pack(rgb);size_t at=py*surface.stride_bytes+px*2;reference[at]=c;reference[at+1]=c>>8;
 }}
}
'''
main=r'''
int main(void){unsigned cases=0;unsigned sizes[]={240,300,328,330,400,466};uint32_t state=7;
for(unsigned z=0;z<6;++z)for(unsigned orient=0;orient<4;++orient)for(unsigned band=0;band<2;++band)for(unsigned test=0;test<120;++test){
 side=sizes[z];inset=(520-side)/2;physical=520;surface.pixels=actual;surface.stride_bytes=1040;flip=orient&1;rotate=orient&2;raster_replaying=band;
 state=state*1664525+1013904223;int x=(int)(state%300)-30;state=state*1664525+1013904223;int y=(int)(state%300)-30;
 state=state*1664525+1013904223;int w=state%270;state=state*1664525+1013904223;int h=state%270;uint32_t color=state;
 raster_band_top=side/3;raster_band_bottom=side*2/3;
 memset(actual,0x5a,sizeof(actual));memset(reference,0x5a,sizeof(reference));failed=false;calls=fail_at=raster_pixels=0;
 old_rect(x,y,w,h,color);assert(sv_native_rect(x,y,w,h,color));assert(!memcmp(actual,reference,sizeof(actual)));++cases;
}
/* Terminal checkpoints never resume writing or invoke another callback. */
side=328;inset=69;surface.stride_bytes=1040;flip=rotate=raster_replaying=false;
for(unsigned failure=1;failure<20;++failure){failed=false;calls=raster_pixels=0;fail_at=failure;memset(actual,0x5a,sizeof(actual));sv_native_rect(0,0,240,240,0x1a2b3c);assert(failed&&calls==failure);memcpy(reference,actual,sizeof(actual));sv_native_rect(0,0,240,240,0xffffff);assert(calls==failure&&!memcmp(actual,reference,sizeof(actual)));}
failed=false;fail_at=0;calls=raster_pixels=0;invalidate_at=1;unbound=false;memset(actual,0x5a,sizeof(actual));sv_native_rect(0,0,240,240,0xabcdef);assert(unbound&&calls==1&&!memcmp(reference,actual,sizeof(actual)));invalidate_at=0;unbound=false;
failed=false;fail_at=0;transforms=scales=calls=raster_pixels=0;old_rect(0,0,240,240,0);unsigned oldtransform=transforms,oldscale=scales;
transforms=scales=0;sv_native_rect(0,0,240,240,0);printf("PASS %u raster parity cases;19 terminal fences;successful-checkpoint unbind fence;328-square transforms %u -> %u,scales %u -> %u\n",cases,oldtransform,transforms,oldscale,scales);
}
'''
(o/'proof.c').write_text(preamble+f+main)
rows=[]
for san in (False,True):
 exe=o/('san' if san else 'normal');cmd=['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(r/'lib/PortableApps/include'),str(o/'proof.c'),'-o',str(exe)]
 if san:cmd+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie']
 subprocess.run(cmd,check=True);v=subprocess.check_output([str(exe)],text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});print(v);rows.append({'command':cmd,'result':v});exe.unlink()
(o/'proof.json').write_text(json.dumps(rows,indent=2))

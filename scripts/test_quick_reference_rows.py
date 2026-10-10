#!/usr/bin/env python3
"""Exercise exact Quick reference primitives: coverage and pre-clip work cost."""
import argparse,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--sanitize',action='store_true')
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
source=ROOT/'lib/PortableApps/src/quick_reference.inc'
primitives=source.read_text().split('static void qr_slider(',1)[0]
primitives=primitives.replace('"../quick_fonts/reference.inc"',json.dumps(str(ROOT/'lib/PortableApps/quick_fonts/reference.inc')))
fixture=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define PORTABLE_RASTER_SNAPSHOT
#define PORTABLE_PAPER_TRANSITIONS
enum {RS_QR_TEXT,RS_QR_ROUND,RS_QR_ICON};
static bool raster_replaying;
static int raster_band_top,raster_band_bottom,qp_offset,screen_height=800;
static uint8_t pixels[480*1024],expected[sizeof(pixels)];
static unsigned rectangles;
static int height(void){return screen_height;}
static bool raster_record(int kind,const int32_t*a,const char*t){(void)kind;(void)a;(void)t;return false;}
static void raster_rows(int64_t origin,int*first,int*end){
 if(!raster_replaying)return;
 int64_t top=(int64_t)raster_band_top-origin,bottom=(int64_t)raster_band_bottom-origin;
 if(top>*first)*first=top<*end?(int)top:*end;
 if(bottom<*end)*end=bottom>*first?(int)bottom:*first;
}
static void qp_rect(int x,int y,int w,int h,bool black){
 ++rectangles;y=(y+qp_offset)*height()/800;h=h*height()/800;
 for(int row=y;row<y+h;row++)for(int col=x;col<x+w;col++)
  if(row>=0&&row<height()&&col>=0&&col<480&&
     (!raster_replaying||(row>=raster_band_top&&row<raster_band_bottom)))
   pixels[row*480+col]=(uint8_t)black;
}
'''+primitives+r'''
static void draw(void){
 qr_round(-10,297,208,96,20,true);qr_round(-6,301,200,88,16,false);
 for(unsigned i=0;i<9;i++)qr_icon(i,(int)i*52,340+i*43,true);
 qr_text(&QUICK_REF_TITLE,32,58,"QUICK ACTIONS",128,0,260,true);
 qr_text(&QUICK_REF_TIME,448,58,"10:59",0,1,0,true);
 qr_text(&QUICK_REF_TILE,180,412,"CONTEXT",32,2,0,true);
 qr_text(&QUICK_REF_TILE,180,435,"DETECTION",32,2,0,true);
 qr_text(&QUICK_REF_SMALL,240,731,"SWIPE UP TO CLOSE",0,2,0,true);
 qr_text(&QUICK_REF_NUMBER,448,764,"93%",0,1,0,true);
}
int main(void){
 unsigned comparisons=0;
 const int offsets[]={-800,-791,-650,-413,-197,-27,0,23};
 const int heights[]={240,480,800,1024};
 for(unsigned hi=0;hi<4;hi++)for(unsigned oi=0;oi<8;oi++){
  screen_height=heights[hi];qp_offset=offsets[oi];
  memset(pixels,0xa5,sizeof(pixels));raster_replaying=false;rectangles=0;draw();
  unsigned full=rectangles;memcpy(expected,pixels,sizeof(pixels));
  for(unsigned band=1;band<=8;band+=7){
   memset(pixels,0xa5,sizeof(pixels));raster_replaying=true;rectangles=0;
   for(raster_band_top=0;raster_band_top<height();raster_band_top+=(int)band){
    raster_band_bottom=raster_band_top+(int)band;if(raster_band_bottom>height())raster_band_bottom=height();draw();
   }
   assert(!memcmp(expected,pixels,sizeof(pixels)));
   // At native logical size every source row is scanned at most once across
   // the whole frame, independent of replay band count and sheet offset.
   if(height()==800)assert(rectangles<=full);
   if(height()==800&&qp_offset==0)printf("Quick reference band=%u: full=%u sliced=%u primitive fills (legacy upper work=%u)\n",band,full,rectangles,full*((height()+band-1)/band));
   ++comparisons;
  }
 }
 printf("Quick reference: %u full-buffer comparisons, native/scaled/offscreen/partial sheet; bounded source-row work PASS\n",comparisons);return 0;
}
'''
f=out/'fixture.c';f.write_text(fixture)
cmd=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror']
if a.sanitize:cmd+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
cmd += [str(f),'-o',str(out/'test')]
subprocess.run(cmd,check=True)
subprocess.run([str(out/'test')],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})

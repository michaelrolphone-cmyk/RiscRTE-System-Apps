#!/usr/bin/env python3
"""Exact bilinear pixel-call and mask/failure-prefix parity against pinned scalar source."""
from pathlib import Path
import argparse,subprocess,json,os
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();r=Path(__file__).resolve().parents[1];o=a.output;o.mkdir(parents=True,exist_ok=True)
path='lib/PortableApps/src/nova.inc';old=subprocess.check_output(['git','-C',r,'show','52fb7cba57d11603529a981f7900078d488ccc2a:'+path],text=True);new=(r/path).read_text()
def fn(s,name):
 start=s.index('static '+('unsigned' if name=='np_coverage' else 'bool' if name=='np_icon' else 'void')+' '+name+'(');b=s.index('{',start);end=b+1;depth=1
 while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[start:end]
pre='''#include <assert.h>\n#include <stdbool.h>\n#include <time.h>\n#include <stdint.h>\n#include <stdlib.h>\n#include <stdio.h>\n#include <string.h>\n#define PORTABLE_SPRINGBOARD_TOUCH_SCROLL\n#define PORTABLE_RASTER_SNAPSHOT\n'''+f'#include "{r}/lib/PortableApps/fonts/watch-native-icons.inc"\n#include "{r}/lib/PortableApps/fonts/springboard_gamepad.inc"\n'+'''enum{RS_NP_ICON,RS_NP_MASK};static int band_top,band_bottom=10000;static bool raster_record(int op,const int32_t*v,const char*s){(void)op;(void)v;(void)s;return false;}static void raster_rows(int64_t origin,int*first,int*end){if(origin+*first<band_top)*first=band_top-origin;if(origin+*end>band_bottom)*end=band_bottom-origin;if(*end<*first)*end=*first;}
static unsigned calls,expected_calls;static bool compare;struct pixel{int x,y;unsigned a;};static struct pixel expected[65536];
static void np_pixel(int x,int y,uint32_t rgb,unsigned alpha){assert(rgb==0xffffff&&calls<65536);if(compare){assert(calls<expected_calls);assert(expected[calls].x==x&&expected[calls].y==y&&expected[calls].a==alpha);}else expected[calls]=(struct pixel){x,y,alpha};calls++;}
static bool nova_mask_pending;static unsigned checks,stop;static uint16_t pixels[1024*1024],backup[1024*1024];static struct{void*pixels;unsigned stride_bytes;}surface;static int ww,hh;static bool raster_surface_writable(void){return true;}static int width(void){return ww;}static int height(void){return hh;}static bool raster_checkpoint(void){return !stop||++checks!=stop;}static void native_point(int*x,int*y){(void)x;(void)y;}
'''
s=pre+fn(new,'np_coverage')+'\n'+fn(old,'np_icon').replace('np_icon(','reference_icon(',1)+'\n'+fn(new,'np_icon')+'\n'+fn(old,'np_mask').replace('np_mask(','reference_mask(',1)+'\n'+fn(new,'np_mask')
s+='''\nint main(void){unsigned long long recurrence=0;for(unsigned gw=1;gw<=255;gw++)for(unsigned w=1;w<=256;w++){unsigned n=gw*128,q=n/w,e=n%w,step=2*n/w,carry=2*n%w;for(unsigned i=0;i<w;i++){assert(q==(2*i+1)*n/w);q+=step;e+=carry;if(e>=w){e-=w;q++;}recurrence++;}}printf("PASS %llu exact quotient/remainder positions\\n",recurrence);unsigned cases=0;const unsigned sizes[]={1,2,3,7,16,31,64,127,256},alpha[]={0,1,85,170,254,255};
for(unsigned g=0;g<sizeof(rpi_icons)/sizeof(*rpi_icons);g++)for(unsigned z=0;z<9;z++)for(unsigned op=0;op<6;op++)for(unsigned clip=0;clip<2;clip++){
 band_top=clip?103:-1000;band_bottom=clip?119:10000;calls=0;compare=false;assert(reference_icon(133,111,sizes[z],rpi_icons[g].name,alpha[op]));expected_calls=calls;calls=0;compare=true;assert(np_icon(133,111,sizes[z],rpi_icons[g].name,alpha[op]));assert(calls==expected_calls);cases++;}
const unsigned dims[][2]={{160,160},{240,240},{466,466},{410,502}};for(unsigned d=0;d<4;d++)for(unsigned clip=0;clip<2;clip++)for(unsigned fault=0;fault<4;fault++){
ww=dims[d][0];hh=dims[d][1];surface.pixels=pixels;surface.stride_bytes=ww*2;band_top=clip?103:0;band_bottom=clip?119:hh;stop=fault?fault*3:0;for(unsigned i=0;i<1024*1024;i++)pixels[i]=(uint16_t)(i*37);checks=0;nova_mask_pending=true;reference_mask();memcpy(backup,pixels,sizeof pixels);unsigned old_checks=checks;for(unsigned i=0;i<1024*1024;i++)pixels[i]=(uint16_t)(i*37);checks=0;nova_mask_pending=true;np_mask();assert(checks==old_checks&&!memcmp(backup,pixels,sizeof pixels));cases++;}
for(unsigned opt=0;opt<2;opt++){clock_t begin=clock();compare=false;band_top=-1000;band_bottom=10000;for(unsigned repeat=0;repeat<30;repeat++)for(unsigned g=0;g<sizeof(rpi_icons)/sizeof(*rpi_icons);g++){calls=0;if(opt)np_icon(133,111,96,rpi_icons[g].name,170);else reference_icon(133,111,96,rpi_icons[g].name,170);}printf("HOST_ONLY icon96 optimized=%u elapsed_ms=%.3f\\n",opt,1000.0*(clock()-begin)/CLOCKS_PER_SEC);}printf("PASS %u exact bilinear call-stream and mask/failure-prefix cases\\n",cases);}
'''
(o/'fixture.c').write_text(s);cmd=['cc','-std=c11','-O2','-Wall','-Wextra','-Werror']
if a.sanitize:cmd+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie']
cmd += [str(o/'fixture.c'),'-o',str(o/'test')];subprocess.run(cmd,check=True);result=subprocess.check_output([str(o/'test')],text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});print(result,end='');(o/'result.json').write_text(json.dumps({'command':cmd,'result':result,'reference':'52fb7cba57d11603529a981f7900078d488ccc2a','physical':False},indent=2))

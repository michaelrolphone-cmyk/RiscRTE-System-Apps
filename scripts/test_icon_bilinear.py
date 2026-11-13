#!/usr/bin/env python3
"""Exact real-glyph, edge, packed-width and opacity oracle for fused coverage."""
from pathlib import Path
import subprocess,os
import argparse,json
parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);parser.add_argument('--cc',default=os.environ.get('CC','cc'));args=parser.parse_args();p=args.output.resolve();p.mkdir(parents=True,exist_ok=True);r=Path(__file__).resolve().parents[1];n=(r/'lib/PortableApps/src/nova.inc').read_text();h=n[n.index('static inline __attribute__'):n.index('static bool np_icon')]
f='''#include <stdint.h>
#include <stdio.h>
#include <assert.h>
'''+f'#include "{r}/lib/PortableApps/fonts/watch-native-icons.inc"\n'+h+'''
static unsigned ref(const rpi_glyph*g,int x,int y){if(x<0||y<0||x>=g->width||y>=g->height)return 0;unsigned n=y*g->width+x;return(g->bits[n/4]>>(6-2*(n%4)))&3;}
int main(void){unsigned long long count=0;for(unsigned gidx=0;gidx<sizeof(rpi_icons)/sizeof(rpi_icons[0]);gidx++){const rpi_glyph*g=&rpi_icons[gidx];for(int size=1;size<=256;size++){int m=g->width>g->height?g->width:g->height;int w=g->width*size/m,h=g->height*size/m;if(w<1)w=1;if(h<1)h=1;for(int j=0;j<h;j++){int v=((2*j+1)*g->height*128/h)-128;int sy=v>=0?v/256:-1;unsigned fy=v-sy*256;for(int i=0;i<w;i++){int u=((2*i+1)*g->width*128/w)-128;int sx=u>=0?u/256:-1;unsigned fx=u-sx*256;unsigned want=(ref(g,sx,sy)*(256-fx)+ref(g,sx+1,sy)*fx)*(256-fy)+(ref(g,sx,sy+1)*(256-fx)+ref(g,sx+1,sy+1)*fx)*fy;assert(np_bilinear_coverage(g,sx,sy,fx,fy)==want);count++;}}}}
uint8_t bits[2048];for(unsigned i=0;i<sizeof(bits);i++)bits[i]=(uint8_t)(i*73u+19u);
for(unsigned w=1;w<=17;w++){rpi_glyph g={"synthetic",w,7,bits};int xs[]={-2,-1,0,(int)w-1,(int)w,(int)w+1};int ys[]={-2,-1,0,6,7,8};for(unsigned xx=0;xx<6;xx++)for(unsigned yy=0;yy<6;yy++)for(unsigned fx=0;fx<256;fx++)for(unsigned fy=0;fy<256;fy++){int x=xs[xx],y=ys[yy];unsigned a=np_bilinear_coverage(&g,x,y,fx,fy),b=(ref(&g,x,y)*(256-fx)+ref(&g,x+1,y)*fx)*(256-fy)+(ref(&g,x,y+1)*(256-fx)+ref(&g,x+1,y+1)*fx)*fy;assert(a==b);assert((a*0u+98304u)/196608u==0);assert((a*255u+98304u)/196608u==(b*255u+98304u)/196608u);count++;}}
printf("PASS %llu all actual glyph/size bilinear samples\\n",count);}
''';(p/'oracle.c').write_text(f)
for mode,flags in [('normal',['-O2']),('sanitized',['-O1','-g1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie'])]:
 c=[args.cc,'-std=c11','-Wall','-Wextra','-Werror',*flags,str(p/'oracle.c'),'-o',str(p/('oracle-'+mode))];(p/(mode+'-command.json')).write_text(json.dumps(c,indent=2));subprocess.run(c,check=True);v=subprocess.run([str(p/('oracle-'+mode))],capture_output=True,text=True,check=True,env=os.environ|{'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'});(p/('oracle-'+mode+'.log')).write_text(v.stdout+v.stderr);print(mode,v.stdout,flush=True)

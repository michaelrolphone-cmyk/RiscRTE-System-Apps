#!/usr/bin/env python3
# Compare the recovered C row renderer to the original Reader C++ raster.
import argparse,hashlib,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
src=ROOT/'lib/PortableApps/boot_logo/StartupScreen.reference.cpp';s=src.read_text()
assert hashlib.sha256(src.read_bytes()).hexdigest()=='da88f61f129160033ab6d48beae3f43ebd19ba1090c61c7b458cc4a6076ae54f'
constants=s[s.index('constexpr uint8_t kBayer8'):s.index('constexpr uint8_t kLogoLayerEnds')]
original=s[s.index('void setPhysicalPixel'):s.index('const uint8_t* glyphFor')]+s[s.index('struct InkPoint'):s.index('bool waitUntilVideoCanSubmit')]
prefix='#include <cstdint>\n#include <cstddef>\n#include <cstring>\n#include <cstdio>\n#include <cstdlib>\n#define T5_VIDEO_FLAG_ONE_IS_BLACK 1\nconstexpr int kFullCoverage=64,kLogoSize=240,kFrameHeight=320,kLogoBlockCount=9;\nstruct {unsigned width,height,stride_bytes,flags;} videoSurface;\nuint32_t visualTimeMs;\n'
main='int main(int argc,char**argv){if(argc!=4)return 2;videoSurface={800,480,100,1};visualTimeMs=(unsigned)atoi(argv[1]);uint8_t b[48000]={0};drawVideoLogo(b,sizeof(b),9,(uint8_t)atoi(argv[2]),(uint8_t)atoi(argv[2]));FILE*f=fopen(argv[3],"wb");fwrite(b,1,sizeof(b),f);return fclose(f);}\n'
(a.output/'oracle.cpp').write_text(prefix+constants+original+main)
port='#include <stdio.h>\n#include <stdlib.h>\n#include <assert.h>\n#include "'+str(ROOT/'lib/PortableApps/boot_logo/RiscRteBootAnimation.h')+'"\n'+r'''
static uint8_t b[48000];
static bool run(void*c,int x,int y,int width){(void)c;assert(x>=0&&x+width<=480&&y>=0&&y<800);for(int px=x;px<x+width;px++){int dx=y,dy=479-px;b[dy*100+dx/8]|=(uint8_t)(128u>>(dx&7));}return true;}
int main(int argc,char**argv){if(argc!=5)return 2;riscrte_boot_frame frame={480,800,(uint32_t)atoi(argv[1]),(uint8_t)atoi(argv[2])};int band=atoi(argv[4]);for(int y=0;y<800;y+=band)assert(riscrte_boot_render_rows(&frame,run,NULL,y,y+band));FILE*f=fopen(argv[3],"wb");fwrite(b,1,sizeof(b),f);return fclose(f);}
'''
(a.output/'port.c').write_text(port)
subprocess.run(['c++','-std=c++17','-O2',a.output/'oracle.cpp','-o',a.output/'oracle'],check=True)
rows=[]
for san in (False,True):
 flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
 exe=a.output/('port-san' if san else 'port')
 subprocess.run(['cc','-std=c11','-O1','-Wall','-Wextra','-Werror',*flags,a.output/'port.c','-o',exe],check=True)
 for time in (0,70,150,300,400,520,650,779,780,1000,1170,1300,1450,1600,1700,1800):
  for coverage in (64,17,0):
   expected=a.output/'expected.bin';actual=a.output/'actual.bin'
   subprocess.run([a.output/'oracle',str(time),str(coverage),expected],check=True)
   for band in (1,7,800):
    subprocess.run([exe,str(time),str(coverage),actual,str(band)],env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'),check=True)
    assert expected.read_bytes()==actual.read_bytes(),(san,time,coverage,band)
   if not san and coverage==64:(a.output/f'frame-{time:04}.bin').write_bytes(actual.read_bytes())
   rows.append({'sanitized':san,'time_ms':time,'coverage':coverage,'bands':[1,7,800],'sha256':hashlib.sha256(actual.read_bytes()).hexdigest()})
(a.output/'evidence.json').write_text(json.dumps({'hardware_tested':False,'original_source_sha256':hashlib.sha256(src.read_bytes()).hexdigest(),'comparisons':len(rows)*3,'frames':rows},indent=2)+'\n')
print(len(rows)*3,'full-frame comparisons with original Reader raster PASS')

#!/usr/bin/env python3
"""Measure immediate-mode raster dispatch delay with deterministic CPU cost. No hardware timing claim."""
import argparse,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');p.add_argument('--snapshot',action='store_true');p.add_argument('--all-graphics',action='store_true');p.add_argument('--settings-graphics',action='store_true');p.add_argument('--native-paper',action='store_true');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
s=(ROOT/'test/native_apps/portable_handoff_test.c').read_text()
s=s.replace('#include "../../lib/PortableApps/src/adapter.c"','#include '+json.dumps(str(ROOT/'lib/PortableApps/src/adapter.c')))
a0='static unsigned mock_allocations,mock_fail_allocation,mock_live;';assert s.count(a0)==1;s=s.replace(a0,'static unsigned mock_format=RISC_DISPLAY_FORMAT_RGB565;\n'+a0)
a0='.supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565),';assert s.count(a0)==1;s=s.replace(a0,'.supported_formats=RISC_DISPLAY_FORMAT_BIT(mock_format),')
s=s.replace('mock_pixels[240*243],mock_old[240*240]','mock_pixels[483*800],mock_old[240*240]')
s=s.replace('.width=240,.height=240','.width=mock_format==RISC_DISPLAY_FORMAT_MONO1?480:240,.height=mock_format==RISC_DISPLAY_FORMAT_MONO1?800:240')
s=s.replace('.stride_bytes=486,','.stride_bytes=mock_format==RISC_DISPLAY_FORMAT_MONO1?63:mock_format==RISC_DISPLAY_FORMAT_GRAY4?123:486,')
s=s.replace('for(unsigned y=0;y<240;y++)for(unsigned x=240;x<243;x++)assert(mock_pixels[y*243+x]==0xa55a);','if(mock_format==RISC_DISPLAY_FORMAT_RGB565)for(unsigned y=0;y<240;y++)for(unsigned x=240;x<243;x++)assert(mock_pixels[y*243+x]==0xa55a);')
s=s.replace('mock_owned[4]','mock_owned[8192]').replace('i<4','i<8192')
s=s.replace('static void *mock_owned[8192];','static void *mock_owned[8192];\nstatic size_t mock_sizes[8192],mock_bytes,mock_peak;')
s=s.replace('mock_owned[i]=p;mock_live++;return p;','mock_owned[i]=p;mock_sizes[i]=bytes;mock_bytes+=bytes;if(mock_peak<mock_bytes)mock_peak=mock_bytes;mock_live++;return p;')
s=s.replace('mock_owned[i]=NULL;mock_live--;free(p);','mock_bytes-=mock_sizes[i];mock_sizes[i]=0;mock_owned[i]=NULL;mock_live--;free(p);')
s=s.replace('#define malloc mock_malloc','#define malloc mock_malloc\nstatic void *raster_test_memcpy(void*,const void*,size_t);\n#define memcpy raster_test_memcpy')
s=s.replace('#undef malloc','#undef memcpy\n#undef malloc')
if a.native_paper:
 s=s.replace('mock_format==RISC_DISPLAY_FORMAT_MONO1?480:240','mock_format==RISC_DISPLAY_FORMAT_MONO1?800:240').replace('mock_format==RISC_DISPLAY_FORMAT_MONO1?800:240,.stride_bytes','mock_format==RISC_DISPLAY_FORMAT_MONO1?480:240,.stride_bytes')
 s=s.replace('.height=mock_format==RISC_DISPLAY_FORMAT_MONO1?800:240','.height=mock_format==RISC_DISPLAY_FORMAT_MONO1?480:240')
 s=s.replace('mock_format==RISC_DISPLAY_FORMAT_MONO1?63:','mock_format==RISC_DISPLAY_FORMAT_MONO1?103:')
s=s.replace('static bool mock_health(risc_runtime_health_v1 *h){h->uptime_ms=mock_ms;', 'static void raster_test_clock(void);\nstatic bool mock_health(risc_runtime_health_v1 *h){raster_test_clock();h->uptime_ms=mock_ms;')
if a.settings_graphics:
 s=s.replace('static bool mock_acquire(const char *name', 'static bool graphics_rtc_read(void*c,twatch_rtc_time_v1*t){(void)c;*t=(twatch_rtc_time_v1){2026,10,10,6,12,0,0};return true;}\nstatic bool graphics_rtc_write(void*c,const twatch_rtc_time_v1*t){(void)c;(void)t;return false;}\nstatic const twatch_rtc_api_v1 graphics_rtc={.api_version=TWATCH_RTC_API_V1,.struct_size=sizeof(graphics_rtc),.read=graphics_rtc_read,.write=graphics_rtc_write};\nstatic bool mock_acquire(const char *name')
 s=s.replace('assert(version==1 && !instance);', 'if(instance){return false;}if(!strcmp(name,"rtc.clock")){assert(version==2);g->api=&graphics_rtc;mock_grants++;return true;}assert(version==1);')
f=out/'fixture.c';f.write_text(s)
cmd=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-DPORTABLE_RETAINED_RGB565_HANDOFF','-DPORTABLE_FORCE_FULL_FRAMES','-DFRAME_FIXTURE='+json.dumps(str(f)),'-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),str(ROOT/'test/native_apps/portable_raster_latency_test.c'),'-o',str(out/'test')]
if a.settings_graphics:cmd[1:1]=['-DPORTABLE_SETTINGS_APP','-DRASTER_SETTINGS_GRAPHICS']
if a.all_graphics:cmd[1:1]=['-DPORTABLE_NOVA_UI','-DRASTER_ALL_GRAPHICS']
if a.native_paper:cmd[1:1]=['-DPORTABLE_DISPLAY_ROTATION=90']
if a.snapshot:cmd[1:1]=['-DPORTABLE_RASTER_SNAPSHOT']
if a.sanitize:cmd[1:1]=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
(out/'compile-command.json').write_text(json.dumps(cmd,indent=2)+'\n');subprocess.run(cmd,check=True)
r=subprocess.run([str(out/'test')],text=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1','RASTER_OUTPUT':str(out)});(out/'run.log').write_text(r.stdout+r.stderr);print(r.stdout+r.stderr,end='');r.check_returncode()

#!/usr/bin/env python3
"""Compare actual unprepared/prepared Settings replay, including geometry rebinding."""
import sys,os,subprocess,json,hashlib
from pathlib import Path
import argparse
p=argparse.ArgumentParser(description="Exact prepared Settings replay comparison; no device performance claim")
p.add_argument('--output',type=Path,required=True);p.add_argument('--runtime-sdk',type=Path,required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[1];o=a.output.resolve();o.mkdir(parents=True,exist_ok=True)
(o/'tmp').mkdir(exist_ok=True)
sys.path.insert(0,str(r/'scripts'));import test_settings_sliced_frames as t
env=os.environ|{'TMPDIR':str(o/'tmp'),'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'}

main=r'''int main(void){begin();finish_frame();assert(acquire_surface());raster_recording=false;raster_replaying=true;raster_input_slice=true;
unsigned count=0;for(unsigned round=0;round<2;round++)for(unsigned rotate=0;rotate<2;rotate++)for(unsigned face=0;face<6;face++)for(unsigned align=0;align<3;align++)for(unsigned kind=0;kind<2;kind++){
 memset(framebuffer,0x5a,sizeof(framebuffer));paper_rotated=rotate;info.flags=round?64:0;content_view.width=content_view.height=328;
 raster_command c={0};c.kind=kind?RS_SV_BUTTON:RS_SV_TEXT;c.text_present=true;strcpy(c.text,"Fit descenders gjpq LONG \xff ~");
 if(kind){c.args[0]=24;c.args[1]=160;c.args[2]=180;c.args[3]=34;c.args[4]=1;}
 else{c.args[0]=face;c.args[1]=align?120:20;c.args[2]=160;c.args[3]=0xcfe9ee;c.args[4]=18;c.args[5]=align;}
 raster_command_span(&c);
 for(int y=0;y<466;y++){
 /* Rebind geometry while retaining the same command identity. */
 if(y==180){content_view.width=content_view.height=300;info.flags^=64;}
 if(y==250){content_view.width=content_view.height=328;info.flags^=64;}
 raster_band_top=y;raster_band_bottom=y+1;raster_execute(&c);
 }
 printf("%u %016llx\n",count++,hash());assert(!failed);
 /* Fresh initialization / same-address command reuse must not reuse setup. */
 raster_free_commands();
}
/* A prior successful preparation must never permit writes once authority fails. */
raster_command c={.kind=RS_SV_TEXT,.text_present=true,.args={1,20,80,0xffffff,18,0}};strcpy(c.text,"terminal");raster_command_span(&c);raster_band_top=100;raster_band_bottom=101;raster_execute(&c);unsigned long long before=hash();failed=true;raster_execute(&c);assert(hash()==before);failed=false;
raster_input_slice=raster_replaying=false;paper_rotated=false;app_module_fini();fprintf(stderr,"PASS %u dynamic geometry/rotation/button/text cases and terminal no-write\n",count);return 0;}
'''
rows=[]
for san in [False,True]:
 ref=None
 for lane in ['base','prepared']:
  d=o/('parity-'+lane+('-san' if san else ''));d.mkdir(exist_ok=True);t.stage(d);f=(d/'map_test.c').read_text();f=f[:f.index('int main(void)')]+main;(d/'parity.c').write_text(f)
  c=['/usr/bin/cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function',*t.FLAGS,*t.SLICED,'-DPORTABLE_SETTINGS_ROUND_GEOMETRY','-DTEST_SETTINGS_DISPLAY_FLAGS=64','-I'+str(r/'lib/PortableApps/include'),'-I'+str(r/'lib/NativeApps/include'),'-I'+str(a.runtime_sdk.resolve()),'-Wl,--wrap=malloc',str(r/'Apps/settings.c'),str(d/'parity.c'),'-o',str(d/'test')]
  if lane=='prepared':c+=['-DPORTABLE_SETTINGS_PREPARED_TEXT']
  if san:c+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie']
  (d/'command.json').write_text(json.dumps(c));subprocess.run(c,env=env,check=True);v=subprocess.run([d/'test'],env=env,check=True,text=True,capture_output=True);(d/'result.log').write_text(v.stdout+v.stderr);ref=ref or v.stdout;assert ref==v.stdout;rows.append(dict(lane=lane,sanitized=san,hash=hashlib.sha256(v.stdout.encode()).hexdigest()));print(lane,san,v.stderr)
(o/'parity.json').write_text(json.dumps(rows,indent=2))

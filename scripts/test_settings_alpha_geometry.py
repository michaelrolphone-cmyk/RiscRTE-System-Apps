#!/usr/bin/env python3
"""Exact Settings transparent-footprint budgets, surfaces and callback fallback."""
from pathlib import Path
import argparse,subprocess,os,json,hashlib
import test_settings_sliced_frames as t
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--runtime-sdk',type=Path,required=True);a=p.parse_args();r=Path(__file__).resolve().parents[1];o=a.output.resolve();o.mkdir(parents=True,exist_ok=True);(o/'tmp').mkdir(exist_ok=True)
env=os.environ|{'TMPDIR':str(o/'tmp'),'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'}
main=r'''
static uint16_t other[sizeof(framebuffer)/2];
static unsigned hook_mode,hook_calls,checkpoint_calls;static bool probing;
static unsigned long long checkpoint_hash;
static bool probe_checkpoint(void){
 if(!probing)return true;
 ++hook_calls;
 if(hook_calls==2){
  if(hook_mode==1){surface.pixels=other;content_view.width=content_view.height=300;}
  if(hook_mode==2){content_view.width=300;content_view.height=320;}
  if(hook_mode==3){surface_format=RISC_DISPLAY_FORMAT_MONO1;}
  if(hook_mode==4){surface.frame=0;surface.pixels=NULL;}
  if(hook_mode==5){failed=true;return false;}
 }
 return true;
}
static void probe_count(void){if(probing){++checkpoint_calls;checkpoint_hash=checkpoint_hash*1099511628211ull+raster_pixels;}}
static unsigned long long combined_hash(void){unsigned long long h=hash();for(unsigned i=0;i<sizeof(other);i++)h=(h^((unsigned char*)other)[i])*1099511628211ull;return h;}
int main(void){begin();finish_frame();assert(acquire_surface());raster_recording=false;raster_replaying=true;
unsigned cases=0;for(unsigned format=1;format<=3;format++)for(unsigned rotate=0;rotate<2;rotate++)for(unsigned free_row=0;free_row<2;free_row++)for(unsigned change=0;change<6;change++){
 memset(framebuffer,0x5a,sizeof(framebuffer));memset(other,0xa5,sizeof(other));surface.pixels=framebuffer;surface.frame=1;
 surface_format=format;paper_rotated=rotate;paper_flip_ui=(cases&1)!=0;info.flags=(cases&2)?64:0;content_view.x=content_view.y=0;content_view.width=content_view.height=cases%3==0?240:cases%3==1?328:466;
 raster_pixels=cases%2?0xfffffff0u:511u;unsigned initial=raster_pixels;
 failed=false;hook_mode=change;hook_calls=checkpoint_calls=0;checkpoint_hash=0;probing=true;raster_input_slice=free_row;
 raster_command c={.kind=RS_SV_FADE,.args={0,192}};raster_command_span(&c);
 for(int y=0;y<466&&!failed;y++){
  /* Even callback-free rows may observe a new surface/geometry next row. */
  if(y==220){surface.pixels=other;content_view.width=300;content_view.height=320;}
  raster_band_top=y;raster_band_bottom=y+1;raster_execute(&c);
 }
 unsigned long long h=combined_hash();unsigned before=raster_pixels;
 if(failed){raster_execute(&c);assert(before==raster_pixels&&h==combined_hash());}
 printf("%u %llu %u %u %u %llu %u\n",cases++,h,raster_pixels-initial,checkpoint_calls,hook_calls,checkpoint_hash,(unsigned)failed);
 probing=false;failed=false;raster_free_commands();
}
surface.pixels=framebuffer;surface.frame=1;surface_format=RISC_DISPLAY_FORMAT_RGB565;raster_input_slice=raster_replaying=false;paper_rotated=false;
app_module_fini();fprintf(stderr,"PASS %u format/rotation/callback/rebind/terminal cases\n",cases);return 0;}
'''
rows=[]
for san in [False,True]:
 ref=None
 for lane in ['base','alpha','both']:
  d=o/(lane+('-san' if san else ''));d.mkdir(exist_ok=True);t.stage(d)
  adapter=(d/'adapter.c').read_text();adapter='static int probe_checkpoint(void);static void probe_count(void);\n'+adapter
  adapter=adapter.replace('static bool raster_checkpoint(void) {','static bool raster_checkpoint(void) {probe_count();')
  adapter=adapter.replace('if(raster_input_slice)return !failed;','if(raster_input_slice)return !failed;\n if(!probe_checkpoint())return false;')
  (d/'adapter.c').write_text(adapter)
  f=(d/'map_test.c').read_text();f=f[:f.index('int main(void)')]+main.replace('static bool probe_checkpoint(void)','static int probe_checkpoint(void)');(d/'probe.c').write_text(f)
  c=['cc','-std=c11','-O1','-g1','-Wall','-Wextra','-Werror','-Wno-unused-function',*t.FLAGS,*t.SLICED,'-DPORTABLE_SETTINGS_ROUND_GEOMETRY','-DPORTABLE_PAPER_PREFERENCES','-DTEST_SETTINGS_DISPLAY_FLAGS=64','-I'+str(r/'lib/PortableApps/include'),'-I'+str(r/'lib/NativeApps/include'),'-I'+str(a.runtime_sdk),'-Wl,--wrap=malloc',str(r/'Apps/settings.c'),str(d/'probe.c'),'-o',str(d/'test')]
  if lane!='base':c+=['-DPORTABLE_SETTINGS_ALPHA_GEOMETRY']
  if lane=='both':c+=['-DPORTABLE_SETTINGS_PREPARED_TEXT']
  if san:c+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie']
  (d/'command.json').write_text(json.dumps(c));subprocess.run(c,check=True,env=env);v=subprocess.run([d/'test'],capture_output=True,text=True,env=env);(d/'result.log').write_text(v.stdout+v.stderr);assert v.returncode==0,(lane,san,v.stderr)
  ref=ref or v.stdout;assert ref==v.stdout,(lane,san,'raster/budget/checkpoint mismatch');rows.append(dict(lane=lane,sanitized=san,sha256=hashlib.sha256(v.stdout.encode()).hexdigest()));print(lane,san,v.stderr,flush=True)
  (d/'test').unlink()
(o/'receipt.json').write_text(json.dumps(rows,indent=2))

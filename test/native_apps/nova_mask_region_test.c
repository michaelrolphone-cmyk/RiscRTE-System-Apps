/* Actual adapter proof, not a replacement mask implementation. */
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
static uint8_t *provider,*software;static size_t provider_bytes,software_bytes,provider_cleared,software_cleared;
static void *tracked_clear(void *p,int v,size_t n){
 uintptr_t a=(uintptr_t)p,b=(uintptr_t)provider,c=(uintptr_t)software;
 if(provider&&a>=b&&a-b<provider_bytes)provider_cleared+=n;
 if(software&&a>=c&&a-c<software_bytes)software_cleared+=n;
 return memset(p,v,n);
}
#include <stdbool.h>
static bool refuse_software;
static void *tracked_alloc(size_t n){if(refuse_software&&n==software_bytes){refuse_software=false;return NULL;}return malloc(n);}
#define malloc tracked_alloc
#define memset tracked_clear
#define PORTABLE_FORCE_FULL_FRAMES
#define PORTABLE_RASTER_SNAPSHOT
#include "../../lib/PortableApps/src/adapter.c"
#undef memset
#undef malloc
const t5_app_manifest_t portable_catalog[1]={{.compatible=false}};const unsigned portable_catalog_count=0;
static unsigned submitted,acquired,clock_calls,fail_clock;static bool invalid_surface,reject_submit;
static void (*clock_hook)(unsigned);
static bool clock_read(risc_runtime_health_v1*h){++clock_calls;if(clock_hook)clock_hook(clock_calls);if(fail_clock&&clock_calls==fail_clock)failed=true;h->uptime_ms=0;return true;}
static void wait_(uint32_t n){(void)n;}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.health=clock_read,.yield_ms=wait_};
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){return v==1?&runtime:NULL;}
static bool acquire_(void*c,uint32_t f,risc_display_surface_v1*out){(void)c;++acquired;*out=(risc_display_surface_v1){.frame=1,.pixels=provider,.width=info.width,.height=info.height,.stride_bytes=info.width*2+12,.size_bytes=provider_bytes,.pixel_format=f};if(invalid_surface)out->width--;return true;}
static void release_(void*c,risc_display_frame_v1 f){(void)c;(void)f;}
static bool submit_(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*d,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*t){(void)c;(void)f;(void)d;(void)n;(void)o;++submitted;*t=1;return !reject_submit;}
static bool status_(void*c,risc_display_present_token_v1 t,risc_display_present_status_v1*out){(void)c;(void)t;*out=(risc_display_present_status_v1){.state=RISC_DISPLAY_PRESENT_COMPLETE};return true;}
static const risc_display_output_api_v1 display_api={.api_version=1,.struct_size=sizeof(display_api),.acquire=acquire_,.release=release_,.submit=submit_,.present_status=status_};

static unsigned random_state=0x73519a2u;
static unsigned random_next(void){random_state=random_state*1664525u+1013904223u;return random_state;}
static void run(unsigned width_,unsigned height_,unsigned pattern,unsigned seed,unsigned fault,bool enable,uint8_t*out,unsigned*callbacks,unsigned*submits,bool*did_fail){
 info=(risc_display_info_v1){.width=width_,.height=height_};surface_format=RISC_DISPLAY_FORMAT_RGB565;paper_rotated=false;failed=false;

 provider_bytes=(width_*2+12)*height_;software_bytes=width_*height_*2;provider=malloc(provider_bytes+64);software=malloc(software_bytes);assert(provider&&software);memset(provider,0xa5,provider_bytes+64);memset(software,0x5a,software_bytes);
 raster_offscreen=fault==99?NULL:software;refuse_software=fault==99;provider_cleared=software_cleared=submitted=acquired=clock_calls=0;fail_clock=fault==99?0:fault;invalid_surface=reject_submit=false;
 surface=(risc_display_surface_v1){0};paper_token=0;raster_begin_allowed=true;raster_recording=raster_sealed=raster_replaying=raster_started=false;
 random_state=seed;raster_pixels=0;np_begin();
 for(unsigned i=0;i<(pattern==0?0:12);i++){
  int x=(int)(random_next()%(width_*3))-((int)width_);int y=(int)(random_next()%(height_*3))-((int)height_);int radius=1+(int)(random_next()%128);unsigned opacity=random_next()%256;
  np_circle(x,y,radius,random_next()&0xffffff,opacity);
  assert(np_icon(x,y,1+(int)(random_next()%256),"solid:f013",(uint8_t)opacity));
  np_pixel(x+1,y+1,0x1ab37c,opacity);
 }
 if(pattern==2)fill(1,2,3,4,0x1234); /* Partial second clear: fallback. */
 if(pattern==3)assert(raster_record(999,(int32_t[8]){0},NULL));
 if(pattern==4){raster_commands->args[4]=0xffff;} /* Nonblack base: fallback. */
 if(pattern==5)raster_commands->args[2]--; /* Incomplete initial clear. */
 np_mask();assert(raster_tail&&raster_tail->kind==RS_NP_MASK);
 bool eligible=pattern<2;assert((raster_tail->args[4]==1)==eligible);
 if(!enable)raster_tail->args[4]=0; /* Original whole-mask oracle for same stream. */
 present(false);for(unsigned i=0;raster_sealed&&!failed&&i<10000;i++)assert(raster_progress()||failed);
 memcpy(out,provider,provider_bytes+64);*callbacks=clock_calls;*submits=submitted;*did_fail=failed;
 if(fault!=99 && failed)assert(!submitted);if(!failed)assert(submitted==1);
 for(unsigned y=0;y<height_;y++)for(unsigned x=width_*2;x<width_*2+12;x++)assert(provider[y*(width_*2+12)+x]==0xa5);
 for(unsigned x=0;x<64;x++)assert(provider[provider_bytes+x]==0xa5);
 failed=false;raster_free_commands();raster_sealed=raster_recording=raster_replaying=false;raster_offscreen=NULL;surface=(risc_display_surface_v1){0};free(provider);free(software);provider=software=NULL;
}
static void identity_hook(unsigned call){
 if(call==2){surface.frame=2;uint16_t white=0xffff;memcpy((uint8_t*)surface.pixels+24*surface.stride_bytes,&white,2);}
 if(call==3)surface.frame=1;
}
static void identity_change(void){
 enum{W=160,H=160};uint8_t oracle[W*H*2],actual[W*H*2];
 info=(risc_display_info_v1){.width=W,.height=H};surface_format=RISC_DISPLAY_FORMAT_RGB565;paper_rotated=false;const int32_t bounds[8]={40,40,60,60,1,W,H,0};
 for(unsigned selected=0;selected<2;selected++){
  uint8_t*p=selected?actual:oracle;memset(p,0,sizeof oracle);surface=(risc_display_surface_v1){.frame=1,.pixels=p,.width=W,.height=H,.stride_bytes=W*2,.size_bytes=sizeof oracle,.pixel_format=RISC_DISPLAY_FORMAT_RGB565};
  raster_replaying=raster_recording=false;nova_mask_pending=true;failed=false;fail_clock=clock_calls=input_sampled_at=0;clock_hook=identity_hook;np_mask_region(selected?bounds:NULL);clock_hook=NULL;assert(!failed&&surface.frame==1);
 }
 assert(!memcmp(oracle,actual,sizeof oracle));surface=(risc_display_surface_v1){0};
}
static void proof_edges(void){
 info=(risc_display_info_v1){.width=466,.height=466};surface_format=RISC_DISPLAY_FORMAT_RGB565;paper_rotated=raster_saved_rotation=false;
 raster_command base={.kind=RS_FILL,.args={0,0,466,466,0}},shape={.kind=RS_NP_CIRCLE,.args={INT32_MIN,INT32_MAX,256}},mask={.kind=RS_NP_MASK};base.next=&shape;raster_commands=&base;
 raster_nova_mask_bounds(&mask);assert(mask.args[4]==1&&mask.args[0]==466&&mask.args[2]==0);
 for(unsigned n=0;n<10;n++){
  mask=(raster_command){.kind=RS_NP_MASK};base=(raster_command){.kind=RS_FILL,.args={0,0,466,466,0},.next=&shape};shape=(raster_command){.kind=RS_NP_PIXEL,.args={10,20}};
  if(n==0)base.args[2]--;if(n==1)base.args[4]=1;if(n==2)shape.kind=RS_BITMAP;if(n==3)shape.paper_offset=1;if(n==4)base.clipped=true;if(n==5)mask.clipped=true;if(n==6)mask.paper_offset=1;if(n==7)paper_rotated=true;if(n==8)raster_saved_rotation=true;if(n==9)surface_format=RISC_DISPLAY_FORMAT_MONO1;
  raster_nova_mask_bounds(&mask);assert(mask.args[4]==0);paper_rotated=raster_saved_rotation=false;surface_format=RISC_DISPLAY_FORMAT_RGB565;
 }
 raster_commands=NULL;
}
int main(void){proof_edges();rt=&runtime;display=&display_api;identity_change();unsigned count=0;const unsigned dims[][2]={{160,160},{240,243},{300,330},{466,466},{480,800}};
 for(unsigned d=0;d<5;d++)for(unsigned p=0;p<6;p++)for(unsigned sample=0;sample<4;sample++){
  unsigned w=dims[d][0],h=dims[d][1];size_t n=(w*2+12)*h+64;uint8_t*a=malloc(n),*b=malloc(n);assert(a&&b);unsigned ca,cb,sa,sb;bool fa,fb;
  for(unsigned f=0;f<5;f++){unsigned fault=f==4?99:f?f*13:0;run(w,h,p,0x19b3+sample,fault,false,a,&ca,&sa,&fa);run(w,h,p,0x19b3+sample,fault,true,b,&cb,&sb,&fb);if(ca!=cb||sa!=sb||fa!=fb){fprintf(stderr,"MISMATCH w=%u h=%u pattern=%u sample=%u fault=%u clocks=%u/%u submits=%u/%u fail=%d/%d\n",w,h,p,sample,fault,ca,cb,sa,sb,fa,fb);abort();}assert(!memcmp(a,b,n));++count;}
  free(a);free(b);
 }
 raster_discard();printf("Mask region actual replay/materialization/reused-command/failure comparisons: %u PASS\n",count);return 0;}

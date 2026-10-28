/* Full production adapter, existing host provider fixture, exact np_pixel oracle. */
#define main inherited_fixture_main
#include FRAME_FIXTURE
#undef main
#include <stddef.h>
#include <limits.h>
#define BYTES 12000
static uint8_t buffers[2][BYTES],expected[2][BYTES];
static union {max_align_t align;uint8_t bytes[BYTES+128];} layer_space;
static risc_runtime_api_v1 row_rt;
static unsigned retained_calls;static bool retain_mock(void){retained_calls++;return true;}
static uint32_t checkpoint_hash[4];
static unsigned callback_mode,checkpoint_calls;static bool callback_enabled,changed;
static bool row_checkpoint_hook(void){
 if(!callback_enabled)return true;
 assert(!native_custody_retained);
 assert(checkpoint_calls<4);uint32_t hash=2166136261u;for(unsigned b=0;b<2;b++)for(unsigned i=0;i<BYTES;i++)hash=(hash^buffers[b][i])*16777619u;checkpoint_hash[checkpoint_calls++]=hash;assert(!changed||!failed);
 if(changed)return true;
 changed=true;
 switch(callback_mode){
 case 1:surface.pixels=buffers[1]+1;break;
 case 2:surface.frame=0;break;
 case 3:surface.stride_bytes=151;break;
 case 4:info.width=48;info.height=24;surface.width=48;surface.height=24;surface.stride_bytes=101;content_view=(portable_safe_viewport){2,2,44,20};break;
 case 5:surface_format=RISC_DISPLAY_FORMAT_MONO1;surface.pixel_format=surface_format;break;
 case 6:return false;
 case 7:failed=true;break;
 case 9:portable_adapter_retain();break;
 case 11:raster_clip_active=true;raster_clip=(portable_scroll_viewport){17,9,7,4};break;
 case 12:raster_clip_active=false;break;
 case 13:paper_rotated=!paper_rotated;break;
 case 14:
#ifdef PORTABLE_PAPER_PREFERENCES
 paper_flip_ui=!paper_flip_ui;
#endif
 break;
 case 15:surface.frame=2;break;
 case 8:surface.frame=0;raster_replaying=true;raster_offscreen=buffers[1]+1;surface.pixels=raster_offscreen;break;
 default:break;
 }return true;
}
static uint32_t random_state=42;static uint32_t rng(void){random_state=random_state*1664525u+1013904223u;return random_state;}
static void setup(unsigned prefix,unsigned mode,unsigned rotation,bool clip,bool band){
 for(unsigned n=0;n<2;n++)for(unsigned i=0;i<BYTES;i++)buffers[n][i]=(uint8_t)(i*73+n*29);
 failed=false;native_custody_retained=false;retained_calls=0;row_rt=mock_rt;row_rt.retain_invocation=retain_mock;rt=native_custody_runtime=&row_rt;mock_ms=input_sampled_at=0;raster_pixels=prefix;checkpoint_calls=0;changed=false;callback_mode=mode;callback_enabled=true;
 info.width=64;info.height=32;surface=(risc_display_surface_v1){.frame=1,.pixels=buffers[0]+1,.width=64,.height=32,.stride_bytes=145,.size_bytes=BYTES-2,.pixel_format=RISC_DISPLAY_FORMAT_RGB565};surface_format=RISC_DISPLAY_FORMAT_RGB565;
 content_view=(portable_safe_viewport){3,4,57,24};paper_rotated=!!(rotation&1);
#ifdef PORTABLE_PAPER_PREFERENCES
 paper_flip_ui=!!(rotation&2);
 if(paper_flip_ui&&!paper_rotated)content_view=(portable_safe_viewport){0,0,64,32};
#endif
 raster_recording=false;raster_replaying=band;raster_offscreen=NULL;raster_layer_paint=NULL;raster_band_top=8;raster_band_bottom=20;
 if(mode==10){surface_format=RISC_DISPLAY_FORMAT_MONO1;surface.pixel_format=surface_format;}
 raster_clip_active=clip;raster_clip=(portable_scroll_viewport){5,7,29,14};
}
static void differential(unsigned prefix,unsigned mode,unsigned rotation,bool clip,bool band,int y,uint32_t color){
 unsigned alpha[700];int x[700];for(unsigned i=0;i<700;i++){alpha[i]=i%7==0?0:i%7==1?255:i%7==2?300:rng()%255;x[i]=(int)(rng()%85)-10;}
 setup(prefix,mode,rotation,clip,band);
 for(unsigned i=0;i<700;i++)np_pixel(x[i],(int)y,color,alpha[i]);
 memcpy(expected,buffers,sizeof(buffers));unsigned count=raster_pixels,checkpoints=checkpoint_calls;bool was_failed=failed;unsigned retained=retained_calls;uint32_t prefixes[4];memcpy(prefixes,checkpoint_hash,sizeof(prefixes));
 setup(prefix,mode,rotation,clip,band);np_rgb565_row row;np_rgb565_row_begin(&row,(int)y,color);
 for(unsigned i=0;i<700;i++)np_rgb565_row_pixel(&row,x[i],alpha[i]);
 if(memcmp(expected,buffers,sizeof(buffers))||count!=raster_pixels||checkpoints!=checkpoint_calls||failed!=was_failed)fprintf(stderr,"row mismatch prefix=%u mode=%u rotation=%u clip=%u band=%u y=%d counts=%u/%u checkpoints=%u/%u\n",prefix,mode,rotation,clip,band,y,count,raster_pixels,checkpoints,checkpoint_calls);
 assert(!memcmp(expected,buffers,sizeof(buffers))&&count==raster_pixels&&checkpoints==checkpoint_calls&&failed==was_failed&&retained_calls==retained);assert(!memcmp(prefixes,checkpoint_hash,checkpoints*sizeof(*prefixes)));
}
int main(void){
 for(unsigned prefix=0;prefix<512;prefix++)for(unsigned mode=0;mode<16;mode++)differential(prefix,mode,prefix%4,prefix%3==0,false,10,0xc93e75);
 for(unsigned n=0;n<300;n++)differential(rng()%512,0,rng()%4,rng()%2,rng()%2,rng()%75,rng()&0xffffff);
 for(unsigned prefix=0;prefix<512;prefix++){differential(prefix,0,prefix%4,true,false,INT_MIN,0xffffff);differential(prefix,0,prefix%4,false,false,INT_MAX,0xffffff);}
 /* Software-layer ownership without a provider frame, plus explicit invalidate. */
 setup(0,0,0,false,false);portable_raster_layer *layer=(void*)layer_space.bytes;memset(layer,0,sizeof(layer_space));raster_layer_paint=layer;surface.frame=0;surface.pixels=layer->pixels;
 np_rgb565_row row;np_rgb565_row_begin(&row,10,0xabcdef);np_rgb565_row_pixel(&row,10,255);unsigned px=13,py=14;assert(layer->pixels[py*145+px*2]||layer->pixels[py*145+px*2+1]);np_rgb565_row_invalidate(&row);surface.pixels=buffers[0]+1;raster_layer_paint=NULL;surface.frame=1;np_rgb565_row_pixel(&row,11,128);
 /* Recording must retain original command behavior, not write the destination. */
 setup(0,0,0,false,false);callback_enabled=false;raster_recording=true;np_rgb565_row_begin(&row,10,0xabcdef);np_rgb565_row_pixel(&row,11,128);raster_recording=false;assert(raster_commands&&raster_commands->kind==RS_NP_PIXEL&&!raster_pixels);raster_free_commands();
 puts("PASS actual adapter row writer: all512 checkpoint prefixes, RGB/alpha/clip/rotation/bands, changed pixels/frame/stride/dimensions/format, failed checkpoints, layer ownership and recording fallback; modeled providers only");}

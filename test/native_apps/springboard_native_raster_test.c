/* Direct native controller/presenter bounds test. It deliberately does not
 * pretend legacy adapter/Quick Controls admit every geometry in this matrix. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define PORTABLE_WATCH_NATIVE_HEIGHT
#define PORTABLE_NOVA_UI
#define count portable_adapter_count
#define launch portable_adapter_launch
#include "../../lib/PortableApps/src/adapter.c"
#undef count
#undef launch
#define springboard_presentation_get unused_presentation_get
#define springboard_transition_active unused_transition_active
#define springboard_catalog_failure_get unused_catalog_failure_get
#include "../../Apps/springboard.c"
#undef springboard_presentation_get
#undef springboard_transition_active
#undef springboard_catalog_failure_get
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){(void)v;return NULL;}
extern const t5_app_manifest_t portable_catalog[];
extern const unsigned portable_catalog_count;
static uint16_t *guarded,*pixels,*reference;static unsigned stride,submissions;
static void begin_native(void){surface.frame=1;nova_mask_pending=true;for(unsigned y=0;y<info.height;y++)for(unsigned x=0;x<info.width;x++)pixels[y*stride+x]=0;}
static void present_native(bool full){(void)full;submissions++;}
static bool manifest_native(uint32_t at,t5_app_manifest_t*out){if(at>=portable_catalog_count)return false;*out=portable_catalog[at];return true;}
static const t5_app_api_v1 native_api={.screen_width=width,.screen_height=height,.installed_apps_get=manifest_native,.present=present_native};
static void check_guards(void){assert(guarded[0]==0xa55a&&guarded[info.height*stride+1]==0xa55a);for(unsigned y=0;y<info.height;y++)for(unsigned x=info.width;x<stride;x++)assert(pixels[y*stride+x]==0xa55a);}
static void save(const char *directory){char path[1024];snprintf(path,sizeof(path),"%s/native-%ux%u.ppm",directory,info.width,info.height);FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%u %u\n255\n",info.width,info.height);for(unsigned y=0;y<info.height;y++)for(unsigned x=0;x<info.width;x++){unsigned p=pixels[y*stride+x];unsigned char rgb[]={(p>>11)*255/31,((p>>5)&63)*255/63,(p&31)*255/31};assert(fwrite(rgb,1,3,f)==3);}assert(!fclose(f));}
static void reference_circle(int x,int y,int radius,uint32_t rgb,unsigned opacity){for(int j=-radius;j<=radius;j++)for(int i=-radius;i<=radius;i++){int edge=radius*16-(int)np_root((unsigned)(i*i+j*j)*256u);unsigned alpha=edge>=16?opacity:edge>0?(unsigned)edge*opacity/16:0;np_pixel(x+i,y+j,rgb,alpha);}}
static void all_glyphs(void) {
 const unsigned sizes[]={1,2,7,16,32,52,64,96,128};
 unsigned highwater=0,checks=0;
 assert(np_vector_unpack());
 for(unsigned i=0;i<sizeof(np_vector_glyphs)/sizeof(np_vector_glyphs[0]);i++) {
  const np_vector_glyph *g=&np_vector_glyphs[i];
  for(unsigned k=0;k<sizeof(sizes)/sizeof(sizes[0]);k++) {
   int size=(int)sizes[k];
   begin_native();assert(np_vector_draw(10,10,size,size,g,0xffffff,255));
   if(np_vector_edges_used>highwater)highwater=np_vector_edges_used;
   check_guards();memcpy(reference,pixels,(size_t)stride*info.height*2);
   begin_native();assert(np_vector_draw(10,10,size,size,g,0xffffff,255));
   assert(!memcmp(reference,pixels,(size_t)stride*info.height*2));
   assert(np_vector_draw(-size/2,-size/2,size,size,g,0xabcdef,173));check_guards();checks++;
  }
 }
 printf("PASS native vectors:%u glyph-size checks, repeated-output equality, negative clipping, edge high-water %u/%u\n",checks,highwater,NP_VECTOR_EDGES);
}
static void scaled_motion(void) {
 const sb_point points[]={{0,0},{54,0},{-54,0},{27,46.75f},{-27,46.75f},{27,-46.75f},{-27,-46.75f}};
 const float scales[]={0.5333333f,0.9958333f,1.0f,1.3333333f,1.7083333f,3.3333333f,8.5333333f};
 for(unsigned k=0;k<sizeof(scales)/sizeof(scales[0]);k++)for(unsigned mode=0;mode<4;mode++) {
  float scale=scales[k];sb_point native[7];for(unsigned i=0;i<7;i++)native[i]=(sb_point){points[i].x*scale,points[i].y*scale};
  sb_motion a,b;sb_motion_init(&a,points,7);sb_motion_init(&b,native,7);b.scale=scale;
  sb_motion_begin(&a);sb_motion_begin(&b);
  for(unsigned i=0;i<40;i++) {
   float dx=mode==0?1:mode==1?5:mode==2?-15:12,dy=mode==3?-16:mode==2?12:0.75f;
   sb_motion_drag(&a,dx,dy,20);sb_motion_drag(&b,dx*scale,dy*scale,20);
   assert(sb_abs(a.x-b.x/scale)<.05f && sb_abs(a.y-b.y/scale)<.05f);
  }
  sb_motion_release(&a);sb_motion_release(&b);
  for(unsigned i=0;i<400;i++) {
   sb_motion_step(&a,20);sb_motion_step(&b,20);
   assert(sb_abs(a.x-b.x/scale)<.3f && sb_abs(a.y-b.y/scale)<.3f);
   assert(b.x>=b.min_x-40*scale && b.x<=b.max_x+40*scale);
   assert(b.y>=b.min_y-40*scale && b.y<=b.max_y+40*scale);
  }
  assert(a.settled&&b.settled&&a.target==b.target);
 }
 puts("PASS native motion:28 scaled drag/friction/overscroll/settle paths preserve normalized trajectory within0.3 design units");
}
int main(int argc,char**argv){
 assert(argc==2);scaled_motion();const unsigned dimensions[][2]={{1,1},{64,64},{128,128},{160,160},{239,293},{240,240},{320,160},{480,320},{320,320},{320,390},{410,502},{800,1000},{1024,1024},{2048,2048}};
 for(unsigned k=0;k<sizeof(dimensions)/sizeof(dimensions[0]);k++){
  info.width=dimensions[k][0];info.height=dimensions[k][1];stride=info.width+3;size_t bytes=(size_t)stride*info.height*2;
  guarded=malloc(bytes+4);reference=malloc(bytes);assert(guarded&&reference);pixels=guarded+1;
  for(size_t i=0;i<bytes/2+2;i++)guarded[i]=0xa55a;
  surface=(risc_display_surface_v1){.frame=1,.pixels=pixels,.width=info.width,.height=info.height,.stride_bytes=stride*2,.size_bytes=bytes,.pixel_format=RISC_DISPLAY_FORMAT_RGB565};
  nova_mode=true;api=&native_api;count=portable_catalog_count;catalog_available=true;
  springboard_presentation view=np_view;view.begin=begin_native;
  nova_state s={.view=&view,.pending=-1,.time_known=true,.hour=10,.minute=21};nova_layout(&s);nova_render(&s);check_guards();
  if(info.width>=128)save(argv[1]);
  if(info.width>=128){assert(nova_hit(&s,(int)info.width/2,(int)info.height/2)==0);assert(s.n==19);}
  /* Compare the optimized scanline circle against independent per-pixel
   * distance coverage. This must be exact even at clipped/negative edges. */
  const int radii[]={1,2,7,22,64,128};
  for(unsigned r=0;r<sizeof(radii)/sizeof(radii[0]);r++)for(unsigned a=51;a<=255;a+=102){
   begin_native();for(unsigned y=0;y<info.height;y++)for(unsigned x=0;x<info.width;x++)pixels[y*stride+x]=0x8421;
   memcpy(reference,pixels,bytes);np_circle(5,7,radii[r],0xbad152,(uint8_t)a);surface.pixels=reference;
   reference_circle(5,7,radii[r],0xbad152,a);surface.pixels=pixels;assert(!memcmp(reference,pixels,bytes));check_guards();
  }
  if(info.width==240)all_glyphs();
  free(guarded);free(reference);
  printf("PASS native controller/raster %ux%u: physical canvas, exact circle oracle, clipping and row guards; adapter admission tested separately\n",info.width,info.height);
 }
 assert(submissions==14);return 0;
}

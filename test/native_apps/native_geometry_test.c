/* Real adapter admission, native primitives and ownership; fake peripherals. */
#include <stdint.h>
static unsigned geometry_width,geometry_height,geometry_stride,geometry_size;
#define main inherited_handoff_main
#include FRAME_FIXTURE
#undef main

static void guards(void) {
 for(unsigned y=0;y<geometry_height;y++)
  for(unsigned x=geometry_width*2;x<geometry_stride;x++)
   assert(((uint8_t*)mock_pixels)[y*geometry_stride+x]==0xa5);
}
static void boundaries(void) {
 uint32_t row,bytes;
 assert(!portable_display_geometry(0,1,RISC_DISPLAY_FORMAT_RGB565,NULL,NULL));
 assert(!portable_display_geometry(1,0,RISC_DISPLAY_FORMAT_RGB565,NULL,NULL));
 assert(!portable_display_geometry(65536,1,RISC_DISPLAY_FORMAT_RGB565,NULL,NULL));
 assert(!portable_display_geometry(1,65536,RISC_DISPLAY_FORMAT_MONO1,NULL,NULL));
 assert(!portable_display_geometry(65535,65535,RISC_DISPLAY_FORMAT_RGB565,NULL,NULL));
 assert(portable_display_geometry(65535,65535,RISC_DISPLAY_FORMAT_MONO1,&row,&bytes));
 assert(row==8192 && bytes==536862720);
 assert(portable_display_geometry(9,3,RISC_DISPLAY_FORMAT_MONO1,&row,&bytes));
 assert(row==2 && bytes==6);
 assert(!portable_display_geometry(1,1,RISC_DISPLAY_FORMAT_GRAY8,NULL,NULL));
 risc_display_surface_v1 s={.frame=1,.pixels=&row,.width=9,.height=3,.stride_bytes=5,.size_bytes=15,.pixel_format=RISC_DISPLAY_FORMAT_MONO1};
 assert(portable_display_surface_valid(&s,9,3,RISC_DISPLAY_FORMAT_MONO1));
 s.size_bytes=14;assert(!portable_display_surface_valid(&s,9,3,RISC_DISPLAY_FORMAT_MONO1));
 s.size_bytes=15;s.stride_bytes=1;assert(!portable_display_surface_valid(&s,9,3,RISC_DISPLAY_FORMAT_MONO1));
 s.stride_bytes=UINT32_MAX;s.size_bytes=UINT32_MAX;assert(!portable_display_surface_valid(&s,9,3,RISC_DISPLAY_FORMAT_MONO1));
 s.stride_bytes=5;s.size_bytes=15;s.width=8;assert(!portable_display_surface_valid(&s,9,3,RISC_DISPLAY_FORMAT_MONO1));
 s.width=9;s.frame=0;assert(!portable_display_surface_valid(&s,9,3,RISC_DISPLAY_FORMAT_MONO1));
 s.frame=1;s.pixels=NULL;assert(!portable_display_surface_valid(&s,9,3,RISC_DISPLAY_FORMAT_MONO1));
}
static void admitted(unsigned w,unsigned h) {
 geometry_width=w;geometry_height=h;geometry_stride=w*2+6;geometry_size=geometry_stride*h;
 uint8_t *allocation=malloc(geometry_size+2),*expected=malloc(geometry_size);assert(allocation&&expected);
 memset(allocation,0xa5,geometry_size+2);mock_pixels=(uint16_t*)(allocation+1);
 mock_flags=RISC_DISPLAY_INFO_ASYNC_PRESENT;mock_polls=mock_ms=0;
 assert(app_module_init()==0);handoff_finish();assert(acquire_surface());
 unsigned allocations=mock_allocations;
 portable_nova_native_fill(0,0,w,h,0xffffff);
 for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w*2;x++)assert(allocation[1+y*geometry_stride+x]==255);
 guards();assert(mock_allocations==allocations); /* no completed-view staging */
 fill(0,0,w,h,0);guards();
 /* Wide offscreen inputs, clipped coverage, and radius squares. */
 const int rects[][5]={{INT_MIN,INT_MIN,INT_MAX,INT_MAX,INT_MAX},
  {INT_MAX,INT_MAX,INT_MAX,INT_MAX,INT_MAX},{-1,-1,INT_MAX,INT_MAX,INT_MAX},
  {-9,-7,(int)w+20,(int)h+20,INT_MAX},{0,0,(int)w,(int)h,0}};
 for(unsigned n=0;n<sizeof(rects)/sizeof(rects[0]);n++){
  const int *r=rects[n];fill(r[0],r[1],r[2],r[3],0x071f);guards();
  raster_command c={.kind=RS_NU_NATIVE_ROUND,.args={r[0],r[1],r[2],r[3],r[4],0x27c1ff}};
  memset(surface.pixels,0xa5,geometry_size);raster_replaying=false;raster_execute(&c);memcpy(expected,surface.pixels,geometry_size);
  memset(surface.pixels,0xa5,geometry_size);raster_replaying=true;raster_command_span(&c);
  for(;raster_band_top<raster_band_limit;raster_band_top++){raster_band_bottom=raster_band_top+1;raster_execute(&c);}
  raster_replaying=false;assert(!memcmp(expected,surface.pixels,geometry_size));guards();
 }
 for(unsigned n=0;n<2;n++){
  int pos=n?INT_MAX:INT_MIN;
  portable_nova_native_text(1,pos,pos,INT_MAX,"BOUNDS",0xffffff);
  raster_command c={.kind=RS_NU_NATIVE_TEXT,.args={1,pos,pos,INT_MAX,0xffffff},.text_present=true};
  strcpy(c.text,"BOUNDS");raster_command_span(&c);assert(raster_band_top==raster_band_limit);
 }
 assert(mock_allocations==allocations);assert(allocation[0]==0xa5&&allocation[geometry_size+1]==0xa5);guards();
 app_module_fini();assert(!mock_grants&&!mock_subscriptions&&!mock_frames&&!mock_live);
 free(expected);free(allocation);mock_pixels=NULL;
 printf("native geometry %ux%u admission/direct/replay/guards/cleanup PASS\n",w,h);
}
int main(void) {
 boundaries();
 const unsigned sizes[][2]={{1,1},{128,64},{160,96},{240,240},{410,502},{502,410},{800,480},{1025,256},{256,2049},{4096,16},{65535,1}};
 for(unsigned i=0;i<sizeof(sizes)/sizeof(sizes[0]);i++)admitted(sizes[i][0],sizes[i][1]);
 puts("ABI byte/stride/dimension boundary checks PASS");return 0;
}

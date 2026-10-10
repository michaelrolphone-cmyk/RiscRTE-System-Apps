#define RASTER_LATENCY_NO_MAIN
#include "portable_raster_latency_test.c"
#include "paper_pixel_reference.inc"
static void original(const sbh_font *font,int x,int baseline,const char *s,int tracking,bool right) {
 int width=0;for(unsigned i=0;s[i];i++){const sbh_glyph*g=raster_sbh_lookup(font,(unsigned char)s[i]);if(g)width+=(int)g->advance+(i?tracking*64:0);}
 int pen=x*64-(right?width:0);
 for(unsigned i=0;s[i];i++){
  const sbh_glyph*g=raster_sbh_lookup(font,(unsigned char)s[i]);if(!g)continue;if(i)pen+=tracking*64;
  for(unsigned y=0;y<g->height;y++)for(unsigned xg=0;xg<g->width;xg++){
   unsigned bit=y*g->width+xg;if(g->bits[bit/8]&(128u>>(bit%8)))fill((pen+32)/64+g->left+(int)xg,baseline+g->top+(int)y,1,1,0);
  }
  pen+=g->advance;
 }
}
static void rectangle_reference(int x,int y,int w,int h,uint16_t color) {
 int x0=x<0?0:x,y0=y<0?0:y,x1=x+w,y1=y+h;
 if(x1>width())x1=width();
 if(y1>height())y1=height();
 unsigned l=((color>>11)&31)*299/31+((color>>5)&63)*587/63+(color&31)*114/31;
 for(int j=y0;j<y1;j++)for(int i=x0;i<x1;i++){
  int px=i,py=j;native_point(&px,&py);uint8_t*p=(uint8_t*)surface.pixels+(size_t)py*surface.stride_bytes;
  uint8_t bit=(uint8_t)(0x80u>>(px&7));if(l<500)p[px/8]|=bit;else p[px/8]&=(uint8_t)~bit;
 }
}
int main(void) {
 begin_test();mock_format=RISC_DISPLAY_FORMAT_MONO1;mock_flags=RISC_DISPLAY_INFO_ASYNC_PRESENT|RISC_DISPLAY_INFO_RETAINS_IMAGE;
 assert(!app_module_init());handoff_finish();assert(acquire_surface());
 size_t bytes=surface.size_bytes;unsigned char *expected=malloc(bytes);assert(expected);
 unsigned rect_checks=0;
 const int rectangles[][4]={{-9,-7,79,33},{1,3,1,1},{7,9,22,31},{0,0,800,800},{477,797,90,99},{21,33,90,8}};
 const uint16_t colors[]={0,0xffff,0x9753};
 for(unsigned rotation=0;rotation<4;rotation++){
  paper_rotated=rotation&1;paper_flip_ui=rotation>>1;
  for(unsigned r=0;r<6;r++)for(unsigned c=0;c<3;c++){
   memset(surface.pixels,0xa5,bytes);rectangle_reference(rectangles[r][0],rectangles[r][1],rectangles[r][2],rectangles[r][3],colors[c]);memcpy(expected,surface.pixels,bytes);
   memset(surface.pixels,0xa5,bytes);fill(rectangles[r][0],rectangles[r][1],rectangles[r][2],rectangles[r][3],colors[c]);assert(!memcmp(expected,surface.pixels,bytes));rect_checks++;
   memset(surface.pixels,0xa5,bytes);raster_recording=true;
   fill(rectangles[r][0],rectangles[r][1],rectangles[r][2],rectangles[r][3],colors[c]);
   raster_recording=false;raster_replaying=true;raster_command_span(raster_commands);
   while(raster_band_top<raster_band_limit){raster_band_bottom=raster_band_top+1;raster_execute(raster_commands);++raster_band_top;}
   raster_replaying=false;raster_band_columns=false;raster_free_commands();
   assert(!memcmp(expected,surface.pixels,bytes));
  }
 }
 /* Rotated replay must not poll expensive runtime health per physical pixel.
  * This is a call-count contract, independent of a desktop's fast clock mock. */
 paper_rotated=true;paper_flip_ui=false;
 unsigned health_before=mock_health_calls;
 raster_replaying=true;
 for(int row=0;row<height();row++) {
  raster_band_top=row;raster_band_bottom=row+1;
  fill(0,0,width(),height(),0xffff);
 }
 raster_replaying=false;
 unsigned health_calls=mock_health_calls-health_before;
 assert(health_calls<=((unsigned)width()*height()+511u)/512u+2u);
 printf("Rotated full-screen replay health calls: %u (bounded per 512 pixels) PASS\n",health_calls);
 paper_rotated=false;paper_flip_ui=false;
 printf("Packed MONO1: %u original-pixel complete-buffer comparisons PASS\n",rect_checks);
 unsigned circle_pixels=0,circle_spans=0;
 for(unsigned rotation=0;rotation<4;rotation++) {
  paper_rotated=rotation&1;paper_flip_ui=rotation>>1;
  int radii[]={1,3,10,38,43,47,128};
  for(unsigned r=0;r<sizeof(radii)/sizeof(*radii);r++)for(unsigned ink=0;ink<2;ink++) {
   int radius=radii[r],x=rotation&1?20:100,y=75;
   memset(surface.pixels,0xa5,bytes);
   for(int j=-radius;j<=radius;j++)for(int i=-radius;i<=radius;i++)if(i*i+j*j<=radius*radius){fill(x+i,y+j,1,1,ink?0:0xffff);circle_pixels++;}
   memcpy(expected,surface.pixels,bytes);memset(surface.pixels,0xa5,bytes);
   pp_circle(x,y,radius,ink);circle_spans+=2*radius+1;assert(!memcmp(expected,surface.pixels,bytes));
  }
  const char *labels[]={"SPRINGBOARD", "BT BUTTONS", "12:45", "MiXeD & punctuation!?"};
  for(unsigned t=0;t<4;t++)for(unsigned zoom=0;zoom<2;zoom++)for(unsigned head=0;head<2;head++) {
   unsigned scale=1|(zoom?PAPER_TEXT_CLOCK:PAPER_TEXT_LITERAL);
   memset(surface.pixels,0xa5,bytes);reference_paper_text(-3,64,470,labels[t],scale,head,true);memcpy(expected,surface.pixels,bytes);
   memset(surface.pixels,0xa5,bytes);pp_text(-3,64,470,labels[t],scale,head,true);assert(!memcmp(expected,surface.pixels,bytes));
  }
 }
 paper_rotated=false;paper_flip_ui=false;
 printf("Paper circles: %u pixel calls replaced by %u spans; 56 circle and 64 label full-buffer comparisons PASS\n",circle_pixels,circle_spans);
 const char *times[]={"0:00","10:59","23:59","--:--","1:11","8:88"};
 unsigned original_commands=0,candidate_commands=0;
 for(unsigned t=0;t<6;t++){
  fill(0,0,width(),height(),0xffff);original(&SBH_900,32,58,"APPS",4,false);original(&SBH_700,448,58,times[t],0,true);memcpy(expected,surface.pixels,bytes);
  fill(0,0,width(),height(),0xffff);raster_recording=true;
  unsigned before=mock_allocations;
  portable_springboard_header_text(0,32,58,"APPS",4,false);portable_springboard_header_text(1,448,58,times[t],0,true);
  assert(raster_command_count==2);assert(mock_allocations-before==1);candidate_commands+=raster_command_count;
  raster_recording=false;assert(raster_materialize());assert(!memcmp(expected,surface.pixels,bytes));
 }
 /* Dense generic pixel clients allocate one slab per 32 commands, preserving order. */
 raster_recording=true;unsigned before=mock_allocations;
 for(unsigned i=0;i<2048;i++)fill((int)(i%width()),(int)(i/width()),1,1,i&1?0xffff:0);
 assert(raster_command_count==2048);assert(mock_allocations-before==64);original_commands=raster_command_count;
 raster_recording=false;assert(raster_materialize());free(expected);end_test();assert(!mock_bytes);
 printf("Header: 6 complete-buffer comparisons, %u candidate commands; dense %u commands use 64 allocations PASS\n",candidate_commands,original_commands);
 return 0;
}

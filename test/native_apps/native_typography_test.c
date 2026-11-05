/* Independent pixel oracle around the real adapter; no app migration required. */
#include <stdint.h>
static unsigned geometry_width,geometry_height,geometry_stride,geometry_size;
#define main inherited_handoff_main
#include FRAME_FIXTURE
#undef main
static uint8_t *initial,*expected;
static unsigned comparisons;
#ifdef PORTABLE_TOUCH_SCROLL
static bool oracle_clipped;
static portable_scroll_viewport oracle_clip;
#endif
static uint64_t receipt=1469598103934665603ull;
static void guards(void) {
 unsigned row=surface_format==RISC_DISPLAY_FORMAT_MONO1?(geometry_width+7)/8:geometry_width*2;
 for(unsigned y=0;y<geometry_height;y++)for(unsigned x=row;x<geometry_stride;x++)
  assert(((uint8_t*)surface.pixels)[y*geometry_stride+x]==0xa5);
}
/* This oracle owns coordinate conversion and color blending; it never calls a
 * production rasterizer, clipping helper, native_point or replay-bound helper. */
static void oracle_pixel(int64_t x,int64_t y,uint32_t color,unsigned alpha) {
 int w=paper_rotated?(int)geometry_height:(int)geometry_width;
 int h=paper_rotated?(int)geometry_width:(int)geometry_height;
 if(x<0||y<0||x>=w||y>=h||!alpha)return;
#ifdef PORTABLE_TOUCH_SCROLL
 if(oracle_clipped&&(x<oracle_clip.x||y<oracle_clip.y||x>=(int64_t)oracle_clip.x+oracle_clip.width||y>=(int64_t)oracle_clip.y+oracle_clip.height))return;
#endif
 if(paper_rotated){int64_t old=x;x=y;y=geometry_height-1-old;}
 uint8_t*p=expected+y*geometry_stride;
 if(surface_format==RISC_DISPLAY_FORMAT_MONO1){
  unsigned bit=128u>>(x%8),old=(p[x/8]&bit)?0:255;
  unsigned l=(((color>>16)&255)*299+((color>>8)&255)*587+(color&255)*114)/1000;
  if((old*(255-alpha)+l*alpha)/255<128)p[x/8]|=bit;else p[x/8]&=(uint8_t)~bit;
 }else{
  p+=x*2;unsigned old=p[0]+256u*p[1];
  unsigned r=(((old>>11)*255/31)*(255-alpha)+((color>>16)&255)*alpha)/255;
  unsigned g=((((old>>5)&63)*255/63)*(255-alpha)+((color>>8)&255)*alpha)/255;
  unsigned b=(((old&31)*255/31)*(255-alpha)+(color&255)*alpha)/255;
  unsigned value=(r/8)*2048+(g/4)*32+b/8;p[0]=value;p[1]=value>>8;
 }
}
static void oracle_text(const raster_command*c){
 unsigned face=c->args[0],source=face==6?0:face,scale=face==6?183:256;
 if(face>6||source>=sizeof(rps_text)/sizeof(*rps_text)/95||c->args[3]<=0)return;
 int64_t pen=(int64_t)c->args[1]*16;
 for(unsigned k=0;k<96&&c->text[k];k++){
  unsigned ch=(unsigned char)c->text[k];if(ch<32||ch>126)ch='?';
  const rps_glyph*g=rps_text+source*95+ch-32;int advance=g->advance_q4*scale/256;
  if(pen/16+(advance+15)/16>(int64_t)c->args[1]+c->args[3])break;
  int w=g->width*scale/256,h=g->height*scale/256;if(g->width&&w<1)w=1;if(g->height&&h<1)h=1;
  for(int y=0;y<h;y++)for(int x=0;x<w;x++){
   unsigned sx=x*256/scale,sy=y*256/scale;if(sx>=g->width||sy>=g->height)continue;
   unsigned n=sy*g->width+sx,alpha=((g->bits[n/4]>>(6-2*(n%4)))&3)*85;
   int64_t px=pen/16+g->left*(int)scale/256+x,py=(int64_t)c->args[2]+g->top*(int)scale/256+y;
   if(px>=c->args[1]&&px<(int64_t)c->args[1]+c->args[3])oracle_pixel(px,py,(uint32_t)c->args[4],alpha);
  }
  pen+=advance;
 }
}
static unsigned oracle_coverage(const rpi_glyph*g,int x,int y){
 if(x<0||y<0||x>=g->width||y>=g->height)return 0;
 unsigned n=y*g->width+x;return (g->bits[n/4]>>(6-2*(n%4)))&3;
}
static void oracle_icon(const raster_command*c){
 int size=c->args[2];if(size<1||size>65535)return;
 const rpi_glyph*g=NULL;
 for(unsigned i=0;i<sizeof(rpi_icons)/sizeof(*rpi_icons);i++)if(!strcmp(c->text,rpi_icons[i].name))g=rpi_icons+i;
 if(!g)return;
 int max=g->width>g->height?g->width:g->height,w=g->width*size/max,h=g->height*size/max;
 if(!w)w=1;
 if(!h)h=1;
 int64_t left=(int64_t)c->args[0]+(size-w)/2,top=(int64_t)c->args[1]+(size-h)/2;
 /* Iterate screen pixels, independently of the renderer's local-offset clip. */
 for(int y=0;y<height();y++)for(int x=0;x<width();x++){
  int64_t i=x-left,j=y-top;if(i<0||j<0||i>=w||j>=h)continue;
  int u=(int)((2*i+1)*g->width*128/w)-128,v=(int)((2*j+1)*g->height*128/h)-128;
  int sx=u>=0?u/256:-1,sy=v>=0?v/256:-1;unsigned fx=u-sx*256,fy=v-sy*256;
  uint64_t cover=(uint64_t)oracle_coverage(g,sx,sy)*(256-fx)*(256-fy)+
   (uint64_t)oracle_coverage(g,sx+1,sy)*fx*(256-fy)+
   (uint64_t)oracle_coverage(g,sx,sy+1)*(256-fx)*fy+
   (uint64_t)oracle_coverage(g,sx+1,sy+1)*fx*fy;
  oracle_pixel(x,y,(uint32_t)c->args[3],(unsigned)((cover*255+98304)/196608));
 }
}
static void oracle_fallback(const raster_command*c){
 for(int k=0;k<c->args[2]&&k<128&&c->text[k];k++){
  int ch=(unsigned char)c->text[k];if(ch>='a'&&ch<='z')ch-=32;
  int g=ch>='0'&&ch<='9'?ch-'0':ch>='A'&&ch<='Z'?ch-'A'+10:-1;
  for(int x=0;x<5;x++)for(int y=0;y<7;y++){
   unsigned bits=g>=0?glyphs[g][x]:ch=='-'?8:ch==':'&&x==2?36:ch=='%'?(x==0||x==4?99:8):0;
   if(bits&(1u<<y)){
    unsigned c565=(unsigned)c->args[3];uint32_t rgb=((c565>>11)*255/31)<<16|(((c565>>5)&63)*255/63)<<8|(c565&31)*255/31;
    oracle_pixel((int64_t)c->args[0]+k*6+x,(int64_t)c->args[1]+y,rgb,255);
   }
  }
 }
}
static void check(raster_command*c){
#ifdef PORTABLE_TOUCH_SCROLL
 oracle_clipped=c->clipped;oracle_clip=c->clip;
#endif
 memcpy(expected,initial,geometry_size);memcpy(surface.pixels,initial,geometry_size);
 if(c->kind==RS_NU_NATIVE_TEXT)oracle_text(c);else if(c->kind==RS_NU_NATIVE_ICON)oracle_icon(c);else oracle_fallback(c);
 unsigned allocations=mock_allocations;raster_replaying=raster_recording=false;raster_pixels=0;raster_execute(c);
 if(memcmp(expected,surface.pixels,geometry_size)){
  fprintf(stderr,"direct mismatch kind=%u args=%d,%d,%d,%d,%d mono=%d rotated=%d dimensions=%ux%u\n",c->kind,c->args[0],c->args[1],c->args[2],c->args[3],c->args[4],surface_format==RISC_DISPLAY_FORMAT_MONO1,paper_rotated,geometry_width,geometry_height);
  for(unsigned z=0;z<geometry_size;z++)if(expected[z]!=((uint8_t*)surface.pixels)[z]){fprintf(stderr,"byte %u expected=%u actual=%u\n",z,expected[z],((uint8_t*)surface.pixels)[z]);break;}
  assert(false);
 }guards();unsigned direct=raster_pixels;
 assert(direct<=(unsigned)width()*(unsigned)height()*2u+96u*40u*40u);
 memcpy(surface.pixels,initial,geometry_size);raster_replaying=true;raster_pixels=0;raster_command_span(c);
 for(;raster_band_top<raster_band_limit;raster_band_top++){raster_band_bottom=raster_band_top+1;raster_execute(c);}
 raster_replaying=false;assert(!memcmp(expected,surface.pixels,geometry_size));assert(raster_pixels==direct);guards();
 assert(mock_allocations==allocations);comparisons++;
 for(unsigned i=0;i<geometry_size;i++){receipt^=expected[i];receipt*=1099511628211ull;}
}
static void samples(const char*out){
 if(surface_format!=RISC_DISPLAY_FORMAT_RGB565||paper_rotated)return;
 memset(surface.pixels,0,geometry_size);
 portable_nova_native_text(0,5,5,width()-10,"NATIVE 0123",0x19e3ff);
 if(height()>=96)portable_nova_native_text(1,5,27,width()-10,"Rajdhani Aa 0123",0xcfe9ee);
 if(height()>=200){
  portable_nova_native_text(2,5,49,width()-10,"Rajdhani 12 Aa 0123",0xcfe9ee);
  portable_nova_native_text(3,5,70,width()-10,"Rajdhani 11 Aa 0123",0xcfe9ee);
  portable_nova_native_text(4,5,91,width()-10,"0123 Aa",0xcfe9ee);
  portable_nova_native_text(5,5,127,width()-10,"Rajdhani 22",0xcfe9ee);
  portable_nova_native_text(6,5,157,width()-10,"Orbitron 10 Aa 0123",0xcfe9ee);
 }
 int size=height()<200?16:width()<300?24:40;
 int top=height()<200?height()-size-18:180;
 for(unsigned i=0;i<6;i++)portable_nova_native_icon(5+(int)i*(size+4),top,size,rpi_icons[i].name,0x19e3ff);
 text_color(5,height()-10,"FALLBACK 5X7 42%",128,0x07ff);
 char path[1024];snprintf(path,sizeof path,"%s/sample-%ux%u.ppm",out,geometry_width,geometry_height);FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%u %u\n255\n",geometry_width,geometry_height);
 for(unsigned y=0;y<geometry_height;y++)for(unsigned x=0;x<geometry_width;x++){uint8_t*p=(uint8_t*)surface.pixels+y*geometry_stride+x*2;unsigned v=p[0]+256u*p[1];fputc((v>>11)*255/31,f);fputc(((v>>5)&63)*255/63,f);fputc((v&31)*255/31,f);}
 assert(!fclose(f));
}
static void test_size(unsigned w,unsigned h,const char*out){
 geometry_width=w;geometry_height=h;geometry_stride=w*2+6;geometry_size=geometry_stride*h;
 uint8_t*memory=malloc(geometry_size+2);assert(memory);mock_pixels=(uint16_t*)(memory+1);
 initial=malloc(geometry_size);expected=malloc(geometry_size);assert(initial&&expected);memset(memory,0xa5,geometry_size+2);
 mock_flags=RISC_DISPLAY_INFO_ASYNC_PRESENT;assert(app_module_init()==0);handoff_finish();assert(acquire_surface());
 for(unsigned mono=0;mono<2;mono++)for(unsigned rotated=0;rotated<2;rotated++){
  if(rotated&&!mono)continue; /* Production rotates paper only. */
  surface_format=mono?RISC_DISPLAY_FORMAT_MONO1:RISC_DISPLAY_FORMAT_RGB565;surface.pixel_format=surface_format;paper_rotated=rotated;
  memset(initial,0xa5,geometry_size);unsigned row=mono?(w+7)/8:w*2;
  for(unsigned y=0;y<h;y++)for(unsigned x=0;x<row;x++)initial[y*geometry_stride+x]=(uint8_t)(x*127+y*39);
  const int positions[][2]={{5,5},{-7,-5},{3,(int)height()-7},{(int)width()-4,2},{INT_MIN,INT_MIN},{INT_MAX,INT_MAX}};
  for(unsigned face=0;face<8;face++)for(unsigned pos=0;pos<6;pos++){
   raster_command c={.kind=RS_NU_NATIVE_TEXT,.args={(int)face,positions[pos][0],positions[pos][1],INT_MAX,0x27c1ff},.text_present=true};
   for(unsigned i=0;i<95;i++)c.text[i]=32+i;
   c.text[95]=0;check(&c);
   c.args[3]=13;strcpy(c.text,"A\1\377?i");check(&c);
  }
  for(unsigned i=0;i<sizeof(rpi_icons)/sizeof(*rpi_icons);i++){
   raster_command c={.kind=RS_NU_NATIVE_ICON,.args={i%2?-7:3,i%3?-5:12,1+(int)(i*7%103),0x19e3ff},.text_present=true};strcpy(c.text,rpi_icons[i].name);check(&c);
  }
  const int sizes[]={0,1,256,410,65535,65536,INT_MAX};
  for(unsigned n=0;n<sizeof sizes/sizeof*sizes;n++)for(unsigned pos=0;pos<6;pos++){
   raster_command c={.kind=RS_NU_NATIVE_ICON,.args={positions[pos][0],positions[pos][1],sizes[n],0xa37c1f},.text_present=true};strcpy(c.text,"solid:f013");check(&c);
  }
  raster_command far_edge={.kind=RS_NU_NATIVE_ICON,.args={-65520,-65520,65535,0x19e3ff},.text_present=true};
  strcpy(far_edge.text,"solid:f017");check(&far_edge);
  for(unsigned pos=0;pos<6;pos++){
   raster_command c={.kind=RS_TEXT,.args={positions[pos][0],positions[pos][1],INT_MAX,0x07ff},.text_present=true};strcpy(c.text,"Native 012: ABC-DEF 23%?!");check(&c);
   text(INT_MIN,INT_MAX,"OFFSCREEN");label(INT_MAX,INT_MIN,INT_MAX,"OFFSCREEN");
  }
#ifdef PORTABLE_TOUCH_SCROLL
  raster_command clipped={.kind=RS_NU_NATIVE_TEXT,.args={1,4,2,200,0xffffff},.text_present=true,.clipped=true,.clip={8,8,19,11}};
  strcpy(clipped.text,"CLIPPED");check(&clipped);
  clipped.kind=RS_NU_NATIVE_ICON;clipped.args[0]=0;clipped.args[1]=0;clipped.args[2]=96;clipped.args[3]=0x19e3ff;strcpy(clipped.text,"solid:f013");check(&clipped);
  clipped.kind=RS_TEXT;clipped.args[0]=4;clipped.args[1]=5;clipped.args[2]=128;clipped.args[3]=0x07ff;strcpy(clipped.text,"CLIPPED");check(&clipped);
  oracle_clipped=false;
#endif
  memcpy(surface.pixels,initial,geometry_size);failed=true;raster_pixels=0;
  portable_nova_native_text(1,2,2,200,"FAILED",0xffffff);portable_nova_native_icon(1,1,80,"solid:f013",0xffffff);
  assert(!raster_pixels&&!memcmp(initial,surface.pixels,geometry_size));failed=false;
  assert(!portable_nova_native_icon(0,0,32,"unknown",0xffffff));assert(!portable_nova_native_icon(0,0,32,NULL,0xffffff));
  samples(out);assert(memory[0]==0xa5&&memory[geometry_size+1]==0xa5);
 }
 /* A captured icon owns its name and reuses the existing bounded command
  * storage. Recording never mutates the provider's frame. */
 memcpy(surface.pixels,initial,geometry_size);raster_recording=true;
 char name[]="solid:f017";assert(portable_nova_native_icon(3,5,40,name,0x19e3ff));
 assert(raster_command_count==1&&raster_commands->kind==RS_NU_NATIVE_ICON);
 name[0]='X';assert(!strcmp(raster_commands->text,"solid:f017"));
 assert(!memcmp(initial,surface.pixels,geometry_size));raster_recording=false;
 check(raster_commands);raster_discard();
 paper_rotated=false;app_module_fini();assert(!mock_grants&&!mock_subscriptions&&!mock_frames&&!mock_live);
 free(initial);free(expected);free(memory);mock_pixels=NULL;
 printf("native typography %ux%u RGB565/MONO1 rotated/direct/replay/oracle/guards/no-allocation PASS\n",w,h);
}
int main(int argc,char**argv){
 assert(argc==2);const unsigned sizes[][2]={{128,64},{160,96},{240,240},{410,502},{502,410},{640,240}};
 for(unsigned i=0;i<sizeof sizes/sizeof*sizes;i++)test_size(sizes[i][0],sizes[i][1],argv[1]);
 printf("comparisons=%u receipt=%016llx hardware=not-tested\n",comparisons,(unsigned long long)receipt);return 0;
}

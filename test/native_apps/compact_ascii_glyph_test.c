/* Actual generic renderer; surface only is modeled. No device/font service. */
#define PORTABLE_FORCE_FULL_FRAMES
#include "../../lib/PortableApps/src/adapter.c"
#include <assert.h>
#include <stdio.h>
const t5_app_manifest_t portable_catalog[1]={{.compatible=false}};
const unsigned portable_catalog_count=0;
static bool healthy(risc_runtime_health_v1*h){h->uptime_ms=0;return true;}
static void wait_(uint32_t n){(void)n;}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.health=healthy,.yield_ms=wait_};
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){return v==1?&runtime:NULL;}
/* Frozen 9c2a049 legacy glyph columns, independent of production table. */
static const uint8_t legacy_glyphs[][5] = {
    {62, 81, 73, 69, 62},  {0, 66, 127, 64, 0},   {98, 81, 73, 73, 70},
    {34, 65, 73, 73, 54},  {24, 20, 18, 127, 16}, {39, 69, 69, 69, 57},
    {60, 74, 73, 73, 48},  {1, 113, 9, 5, 3},     {54, 73, 73, 73, 54},
    {6, 73, 73, 41, 30},   {126, 9, 9, 9, 126},   {127, 73, 73, 73, 54},
    {62, 65, 65, 65, 34},  {127, 65, 65, 34, 28}, {127, 73, 73, 73, 65},
    {127, 9, 9, 9, 1},     {62, 65, 73, 73, 58},  {127, 8, 8, 8, 127},
    {0, 65, 127, 65, 0},   {32, 64, 65, 63, 1},   {127, 8, 20, 34, 65},
    {127, 64, 64, 64, 64}, {127, 2, 12, 2, 127},  {127, 4, 8, 16, 127},
    {62, 65, 65, 65, 62},  {127, 9, 9, 9, 6},     {62, 65, 81, 33, 94},
    {127, 9, 25, 41, 70},  {70, 73, 73, 73, 49},  {1, 1, 127, 1, 1},
    {63, 64, 64, 64, 63},  {31, 32, 64, 32, 31},  {127, 32, 24, 32, 127},
    {99, 20, 8, 20, 99},   {3, 4, 120, 4, 3},     {97, 81, 73, 69, 67}};
static unsigned legacy_bits(unsigned char c,unsigned col){
 if(c>='a'&&c<='z')c-=32;
 int g=c>='0'&&c<='9'?c-'0':c>='A'&&c<='Z'?c-'A'+10:-1;
 return g>=0?legacy_glyphs[g][col]:c=='-'?8:c==':'&&col==2?36:c=='%'?(col==0?99:col==4?99:8):0;
}
static uint8_t *allocation;static size_t bytes,stride;static unsigned width_=800,height_=40;
static void reset(void){memset(allocation,0xa5,bytes+128);for(unsigned y=0;y<height_;y++)memset(allocation+64+y*stride,surface_format==RISC_DISPLAY_FORMAT_RGB565?0xff:0,surface_format==RISC_DISPLAY_FORMAT_RGB565?width_*2:(width_+7)/8);}
static void guards(void){for(unsigned i=0;i<64;i++)assert(allocation[i]==0xa5&&allocation[64+bytes+i]==0xa5);for(unsigned y=0;y<height_;y++)for(unsigned x=surface_format==RISC_DISPLAY_FORMAT_RGB565?width_*2:(width_+7)/8;x<stride;x++)assert(allocation[64+y*stride+x]==0xa5);}
static bool ink(unsigned x,unsigned y){uint8_t*p=allocation+64+y*stride;return surface_format==RISC_DISPLAY_FORMAT_RGB565?((uint16_t*)p)[x]!=0xffff:!!(p[x/8]&(0x80u>>(x%8)));}
static unsigned count_ink(void){unsigned n=0;for(unsigned y=0;y<height_;y++)for(unsigned x=0;x<width_;x++)n+=ink(x,y);return n;}
static void exact_plus(int x,int y){for(unsigned yy=0;yy<height_;yy++)for(unsigned xx=0;xx<width_;xx++){bool expected=(yy==(unsigned)y+3&&xx>=(unsigned)x&&xx<(unsigned)x+5)||(xx==(unsigned)x+2&&yy>=(unsigned)y+1&&yy<=(unsigned)y+5);if(ink(xx,yy)!=expected){fprintf(stderr,"format=%u pixel %u,%u actual=%u expected=%u count=%u\n",surface_format,xx,yy,ink(xx,yy),expected,count_ink());abort();}}assert(count_ink()==9);}
static void run(unsigned format){
 rt=&runtime;info=(risc_display_info_v1){.width=width_,.height=height_};surface_format=format;paper_rotated=false;failed=false;
 stride=(format==RISC_DISPLAY_FORMAT_RGB565?width_*2:(width_+7)/8)+16;bytes=stride*height_;allocation=malloc(bytes+128);assert(allocation);
 surface=(risc_display_surface_v1){.frame=1,.pixels=allocation+64,.width=width_,.height=height_,.stride_bytes=stride,.size_bytes=bytes,.pixel_format=format};
 reset();text_color(10,10,"+",1,0);exact_plus(10,10);guards();
 reset();label(20,10,30,"+");exact_plus(32,10);guards();
 unsigned symbols=0;for(unsigned c=33;c<=126;c++)if(!((c>='0'&&c<='9')||(c>='A'&&c<='Z')||(c>='a'&&c<='z'))){char s[2]={(char)c,0};reset();text_color(10,10,s,1,0);assert(count_ink()>0);guards();++symbols;}assert(symbols==32);
 const char *old="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-:% ";
 uint8_t*expected=malloc(bytes);assert(expected);
 for(unsigned i=0;old[i];i++)for(unsigned edge=0;edge<3;edge++){
  int x=edge==0?10:edge==1?-2:(int)width_-3,y=edge==0?10:edge==1?-2:(int)height_-3;
  reset();for(unsigned col=0;col<5;col++)for(unsigned row=0;row<7;row++)if(legacy_bits((unsigned char)old[i],col)&(1u<<row))fill(x+(int)col,y+(int)row,1,1,0);
  memcpy(expected,surface.pixels,bytes);reset();char one[2]={old[i],0};text_color(x,y,one,1,0);assert(!memcmp(expected,surface.pixels,bytes));guards();
 }
 reset();text_color(0,0," \t\r\n\x7f\x80",128,0);assert(!count_ink());
 reset();text_color(0,0,NULL,128,0);text_color(0,0,"+",0,0);text_color(0,0,"+",-1,0);assert(!count_ink());
 reset();text_color(0,0,"++",1,0);assert(count_ink()==9);guards();
 char long_text[128];memset(long_text,'!',sizeof(long_text)); /* Deliberately no NUL: hard cap bounds reads too. */
 reset();text_color(0,0,long_text,200,0);assert(count_ink()==128u*6u);guards();
 free(expected);free(allocation);surface=(risc_display_surface_v1){0};
 printf("PASS compact ASCII format=%u: exact plus/text+label,32 punctuation,198 legacy edge rasters, blank/limits/stride/guards\n",format);
}
int main(void){run(RISC_DISPLAY_FORMAT_RGB565);run(RISC_DISPLAY_FORMAT_MONO1);return 0;}

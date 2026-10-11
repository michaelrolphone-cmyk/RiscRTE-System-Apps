/* Compare complete immediate primitive output against every replay row. The
 * reference keeps raster_replaying false, so none of the new row bounds run. */
#define RASTER_LATENCY_NO_MAIN
#include "portable_raster_latency_test.c"

static void rows_draw(unsigned kind,int y) {
 static const char glyphs[]=" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
 switch(kind) {
  case 0:rounded(-9,y,73,39,17,2);break;
  case 1:text_color(-4,y,glyphs,95,0x9753);break;
  case 2:assert(icon(-4,y,"solid:f013",47,true));break;
  case 3:assert(icon(-4,y,"solid:f013",47,false));break;
  case 4:assert(np_icon(31,y,53,"solid:f013",177));break;
  case 5:np_circle(25,y,38,0xf549a2,171);break;
  case 6:nova_mask_pending=true;np_mask();break;
  case 7:np_caption(y,glyphs,false,0x87bc12);break;
  case 8:np_caption(y,"10:59",true,0xc7efff);break;
  case 9:nu_rect(-7,y,239,29,0x832daf);break;
  case 10:for(unsigned face=0;face<=NOVA_FACE_ORBITRON_10;face++)portable_nova_text(face,-7,y,224,glyphs,0x1256aa);break;
  case 11:portable_nova_round(-7,y,71,38,13,0xfe0987);break;
  case 12:portable_nova_button(-7,y,83,44,"a B 09",true);break;
  case 13:pp_circle(31,y,46,true);pp_circle(31,y,17,false);break;
  case 14:pp_text(-5,y,460,glyphs,1,false,true);break;
  case 15:pp_text(-5,y,460,glyphs,PAPER_TEXT_LITERAL|PAPER_TEXT_CLOCK,true,false);break;
  case 16:sv_rect(-7,y,243,37,0x314159);break;
  case 17:for(unsigned face=0;face<6;face++)sv_text(face,-7,y,glyphs,0x19e3ff,18);break;
  case 18:sv_button(-7,y,112,32,"A button",true);break;
  case 19:sv_fade(y,y+139);break;
  default:assert(!"unknown primitive");
 }
}

static void rows_case(unsigned format) {
 begin_test();mock_format=format;mock_flags=RISC_DISPLAY_INFO_ASYNC_PRESENT|
  (format==RISC_DISPLAY_FORMAT_MONO1?RISC_DISPLAY_INFO_RETAINS_IMAGE:0);
 assert(!app_module_init());handoff_finish();assert(acquire_surface());
 const size_t bytes=surface.size_bytes;
 unsigned char *initial=malloc(bytes),*expected=malloc(bytes);assert(initial&&expected);
 /* Include untouched padding and prior alpha/blend pixels in the comparison. */
 memcpy(initial,surface.pixels,bytes);
 unsigned active=surface_format==RISC_DISPLAY_FORMAT_MONO1?(info.width+7)/8:info.width*2;
 for(unsigned y=0;y<info.height;y++)for(unsigned x=0;x<active;x++)
  initial[(size_t)y*surface.stride_bytes+x]=(unsigned char)((y*active+x)*73u+29u);
 unsigned checked=0;
 const int positions[]={-57,-9,0,17,66,124,142,192,233,260,390,791,817};
 for(unsigned kind=0;kind<20;kind++) {
  if(kind==6&&format!=RISC_DISPLAY_FORMAT_RGB565)continue;
  for(unsigned position=0;position<sizeof(positions)/sizeof(*positions);position++) {
   memcpy(surface.pixels,initial,bytes);nova_mask_pending=false;
   raster_replaying=false;rows_draw(kind,positions[position]);
   memcpy(expected,surface.pixels,bytes);
   for(unsigned band=1;band<=7;band+=6) {
    memcpy(surface.pixels,initial,bytes);nova_mask_pending=false;
    raster_replaying=true;
    for(raster_band_top=0;raster_band_top<height();raster_band_top+=(int)band) {
     raster_band_bottom=raster_band_top+(int)band;
     if(raster_band_bottom>height())raster_band_bottom=height();
     rows_draw(kind,positions[position]);
    }
    raster_replaying=false;
    if(memcmp(expected,surface.pixels,bytes))fprintf(stderr,"row mismatch format=%u kind=%u y=%d band=%u\n",format,kind,positions[position],band);
    assert(!memcmp(expected,surface.pixels,bytes));++checked;
   }
  }
 }
 free(expected);free(initial);end_test();assert(!mock_bytes);
 printf("raster primitive rows: format=%u complete-buffer comparisons=%u PASS\n",format,checked);
}
int main(void) {
 setvbuf(stdout,NULL,_IONBF,0);rows_case(RISC_DISPLAY_FORMAT_RGB565);rows_case(RISC_DISPLAY_FORMAT_MONO1);return 0;
}

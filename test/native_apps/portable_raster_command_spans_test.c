/* Exercise the actual recording/dispatch/span functions, not guessed bounds.
 * Full-height replay is an independent pixel oracle for every captured kind. */
#define RASTER_ROW_BOUNDS_NO_MAIN
#include "portable_raster_row_bounds_test.c"
static unsigned long replay_calls[64];
static void span_replay(bool bounded,bool previous) {
 raster_replaying=true; nova_mask_pending=false;
 for(raster_command*c=raster_commands;c;c=c->next) {
  if(bounded)raster_command_span(c);
  else {raster_band_columns=false;raster_band_top=0;raster_band_limit=height();}
  if(previous&&c->kind==RS_NP_CAPTION){raster_band_top=0;raster_band_limit=height();}
  if(previous&&c->kind==RS_NP_ICON){
   int64_t hi=(int64_t)c->args[1]+c->args[2]+1;
   raster_band_limit=hi<0?0:hi>height()?height():(int)hi;
  }
  for(;raster_band_top<raster_band_limit;++raster_band_top){
   raster_band_bottom=raster_band_top+1;
   ++replay_calls[c->kind];raster_execute(c);assert(!failed);
  }
 }
 raster_replaying=false;
}
static void record_primitive(unsigned kind,int y) {
 raster_free_commands();raster_recording=true;nova_mask_pending=false;
 rows_draw(kind,y);raster_recording=false;
 assert(raster_commands);
}
static void span_case(unsigned format) {
 begin_test();mock_format=format;mock_flags=RISC_DISPLAY_INFO_ASYNC_PRESENT|
  (format==RISC_DISPLAY_FORMAT_MONO1?RISC_DISPLAY_INFO_RETAINS_IMAGE:0);
 assert(!app_module_init());handoff_finish();assert(acquire_surface());
 size_t bytes=surface.size_bytes;unsigned char*initial=malloc(bytes),*expected=malloc(bytes);
 assert(initial&&expected);memcpy(initial,surface.pixels,bytes);
 unsigned active=surface_format==RISC_DISPLAY_FORMAT_MONO1?(info.width+7)/8:info.width*2;
 for(unsigned y=0;y<info.height;y++)for(unsigned x=0;x<active;x++)initial[(size_t)y*surface.stride_bytes+x]=(unsigned char)((y*active+x)*73u+29u);
 const int positions[]={-57,-9,0,17,66,124,142,192,233,260,390,460,791,817};
 unsigned comparisons=0;
#ifndef RASTER_BENCHMARK_ONLY
 for(unsigned kind=0;kind<20;kind++)for(unsigned pos=0;pos<sizeof(positions)/sizeof(*positions);pos++){
  if(kind==6&&format!=RISC_DISPLAY_FORMAT_RGB565)continue;
  record_primitive(kind,positions[pos]);
  memcpy(surface.pixels,initial,bytes);span_replay(false,false);memcpy(expected,surface.pixels,bytes);
  memcpy(surface.pixels,initial,bytes);span_replay(true,false);
  if(memcmp(expected,surface.pixels,bytes))fprintf(stderr,"span mismatch kind=%u y=%d format=%u\n",kind,positions[pos],format);
  assert(!memcmp(expected,surface.pixels,bytes));++comparisons;
 }
 /* Truncation, spaces, empty, NULL, fallback glyphs and both font banks. */
 const char *captions[]={"10:40 AM","STOPWATCH","SETTINGS","  ","",NULL,"lower case !?\xff", "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW"};
 for(unsigned t=0;t<sizeof(captions)/sizeof(*captions);++t)for(unsigned font=0;font<2;++font)for(unsigned p=0;p<sizeof(positions)/sizeof(*positions);p++){
  raster_free_commands();raster_recording=true;nova_mask_pending=false;np_caption(positions[p],captions[t],font,0x87bc12);raster_recording=false;
  memcpy(surface.pixels,initial,bytes);span_replay(false,false);memcpy(expected,surface.pixels,bytes);
  memcpy(surface.pixels,initial,bytes);span_replay(true,false);assert(!memcmp(expected,surface.pixels,bytes));++comparisons;
 }
 /* Unknown commands and known whole-surface work must never get guessed empty
  * spans. They remain safe if their implementation evolves. */
 const unsigned fallback[]={RS_NP_MASK,RS_QA_RENDER,RS_NU_TEXT,RS_NU_BUTTON,RS_WV_TEXT,RS_WV_BUTTON,65535};
 for(unsigned n=0;n<sizeof(fallback)/sizeof(*fallback);n++){
  raster_command c={0};c.kind=fallback[n];raster_command_span(&c);
  assert(raster_band_top==0&&raster_band_limit==height());
 }
#endif
 printf("command spans%s format=%u %dx%d comparisons=%u PASS\n",comparisons?"":" (benchmark only; no parity assertion)",format,width(),height(),comparisons);
 if(format==RISC_DISPLAY_FORMAT_RGB565){
  /* Representative overlapping Springboard frame. Identical command payload,
   * row ordering, starting pixels, checkpoint implementation and pixel work. */
  raster_free_commands();raster_recording=true;nova_mask_pending=false;
  for(int i=0;i<19;i++){
   int x=width()/2+(i%5-2)*63,y=height()/2+(i/5-2)*63;
   np_circle(x,y,31,0x082f3d,210);assert(np_icon(x,y,40,"solid:f013",223));
  }
  nova_mask_pending=true;np_caption(16,"10:40 AM",true,0xffffff);
  np_caption(height()-43,"STOPWATCH",false,0x22ffff);raster_recording=false;
  memcpy(surface.pixels,initial,bytes);span_replay(true,true);memcpy(expected,surface.pixels,bytes);
  memcpy(surface.pixels,initial,bytes);span_replay(true,false);assert(!memcmp(expected,surface.pixels,bytes));
  uint32_t hash=2166136261u;for(size_t n=0;n<bytes;n++)hash=(hash^expected[n])*16777619u;
  printf("representative complete-buffer fnv1a=%08x bytes=%zu\n",hash,bytes);
  for(unsigned previous=0;previous<2;previous++){
   for(unsigned warm=0;warm<8;warm++){memcpy(surface.pixels,initial,bytes);span_replay(true,previous);}
   memset(replay_calls,0,sizeof(replay_calls));clock_t start=clock();
   const unsigned frames=128;
   for(unsigned n=0;n<frames;n++){memcpy(surface.pixels,initial,bytes);span_replay(true,previous);}
   printf("benchmark previous=%u frames=%u host_ms=%.3f caption_calls_per_frame=%lu icon_calls_per_frame=%lu pixel_checksum=%u\n",previous,frames,1000.0*(clock()-start)/CLOCKS_PER_SEC,replay_calls[RS_NP_CAPTION]/frames,replay_calls[RS_NP_ICON]/frames,*(unsigned char*)surface.pixels);
  }
 }
 raster_free_commands();free(expected);free(initial);end_test();assert(!mock_bytes);
}
int main(void){setvbuf(stdout,NULL,_IONBF,0);span_case(RISC_DISPLAY_FORMAT_RGB565);
#ifndef RASTER_RGB466
span_case(RISC_DISPLAY_FORMAT_MONO1);
#endif
return 0;}

/* Root and Time/Date retain independent offsets across nested editors. The
 * shared controller owns bounded drag/momentum. Presses snapshot the current
 * logical position; display completion only controls presentation. */
static void sp_scroll_configure(unsigned page) {
  unsigned reserve=252u;
#ifdef PORTABLE_RESIDENT_SHELL_CLIENT
  if(page==SV_ROOT)reserve=32u; /* Physical Home/Back leave the full list viewport. */
#endif
  portable_scroll_configure(&sp_scroll[page==SV_FIELDS],
      (portable_scroll_viewport){32,SP_TOP,width()-64,height()-SP_TOP-(int)reserve},
      SP_ROW,page==SV_FIELDS?6:SETTINGS_ROW_COUNT);
}
static void sp_scroll_visible(void) {
  if(!paper_token){sp_completed_page=sp_submitted_page;sp_completed_q8=sp_submitted_q8;}
}
static unsigned sp_scroll_begin(unsigned page) {
  sp_scroll_configure(page);
  portable_touch_scroll *scroll=&sp_scroll[page==SV_FIELDS];
  raster_clip=scroll->view;raster_clip_active=true;
  return (unsigned)scroll->view.height/SP_ROW+2;
}
static void sp_scroll_end(unsigned page) {
  portable_touch_scroll *scroll=&sp_scroll[page==SV_FIELDS];
  raster_clip_active=false;
  if(!scroll->limit)return;
  int bar=scroll->view.height*scroll->view.height/(scroll->count*scroll->extent);
  if(bar<24)bar=24;
  int y=scroll->view.y+(scroll->view.height-bar)*portable_scroll_offset(scroll)/scroll->limit;
  fill(width()-24,y,4,bar,0);
}
static void sp_scroll_reveal(unsigned page,unsigned row) {
  sp_scroll_configure(page);portable_scroll_reveal(&sp_scroll[page==SV_FIELDS],(int)row);
}

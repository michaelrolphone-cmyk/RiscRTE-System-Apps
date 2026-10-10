/* Read-only observations; all gestures/navigation use the production loop. */
#include <assert.h>
#include "../../Apps/file_browser.c"
bool native_system_test_open(void){return fb_open();}
const portable_touch_scroll *scroll_files_state(void){return &fbs_scroll;}
unsigned scroll_files_mode(void){return fb_mode;}
unsigned scroll_files_cache(void){return fbs_cache_start;}
unsigned scroll_files_selected(void){return fbs_selected;}
unsigned scroll_files_generation(void){return fbs_generation;}
unsigned scroll_files_complete_generation(void){return fbs_completed.generation;}
int scroll_files_complete_q8(void){return fbs_completed.q8;}
const char *scroll_files_name(void){return fb_selected.name;}
const char *scroll_files_path(void){return fb_path;}
const char *scroll_files_editor(void){return fb_editor;}
const char *scroll_files_query(void){return fb_query;}
bool scroll_files_cleanup(void){return !portable_file_browser_safe();}

/* Arithmetic contract at folder sizes beyond the controller's pixel window.
 * No provider I/O or invented large-folder performance claim is involved. */
void scroll_files_coordinate_checks(void){
 unsigned saved_mode=fb_mode,saved_matched=fb_matched;bool saved_loaded=fb_loaded;
 fb_mode=FB_LIST;fb_loaded=true;fb_matched=UINT32_MAX;fbs_mode=FB_LIST;fbs_origin=0;fbs_scroll=(portable_touch_scroll){0};fbs_prepare();
 assert(fbs_scroll.count==FBS_COORDINATE_ROWS&&fbs_scroll.limit>0);
 fbs_reveal(UINT32_MAX-1);assert(fbs_origin==UINT32_MAX-FBS_COORDINATE_ROWS);
 assert(portable_scroll_row_y(&fbs_scroll,(int)(UINT32_MAX-1-fbs_origin))+FBS_ROW==fbs_scroll.view.y+fbs_scroll.view.height);
 fbs_origin=0;fbs_scroll.position_q8=(int)(FBS_COORDINATE_ROWS*3/4+10)*FBS_ROW*256;
 fbs_scroll.start_q8=fbs_scroll.position_q8;fbs_scroll.contact=true;fbs_configure();
 uint64_t before=(uint64_t)fbs_origin*FBS_ROW*256+(unsigned)fbs_scroll.position_q8;
 fbs_slide_coordinates();assert(fbs_origin>0&&fbs_scroll.contact);
 assert(before==(uint64_t)fbs_origin*FBS_ROW*256+(unsigned)fbs_scroll.position_q8&&fbs_scroll.start_q8==fbs_scroll.position_q8);
 fbs_scroll.position_q8=20*FBS_ROW*256;fbs_scroll.start_q8=fbs_scroll.position_q8;
 before=(uint64_t)fbs_origin*FBS_ROW*256+(unsigned)fbs_scroll.position_q8;
 fbs_slide_coordinates();assert(fbs_origin==0&&before==(unsigned)fbs_scroll.position_q8);
 fb_mode=saved_mode;fb_matched=saved_matched;fb_loaded=saved_loaded;fbs_reset();
}

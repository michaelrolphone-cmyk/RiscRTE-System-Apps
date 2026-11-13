#include <assert.h>
#include <stdio.h>
#include "../../Apps/files_scene_text_pending.h"
#include "T5FileOpenApi.h"
#define FB_PATH_CAP 512u
#define FB_MAX_PAGE_ROWS 6u
static struct {uint64_t token;} files_ui;
static files_text_identity current;
static t5_file_handler_t fb_handlers[6];
static char fb_handler_path[512];
static bool live,pause_ok,close_ok,invalidated;
static unsigned dispatches,selected,messages,closes;
static bool files_scene_alive(void){return live;}
static bool files_scene_background_pause(void){return pause_ok;}
static bool files_ui_close(void *p){(void)p;closes++;if(invalidated)live=false;if(close_ok&&live)files_ui.token=0;return close_ok;}
static bool files_text_snapshot(void*p,files_text_identity*out){(void)p;*out=current;return live;}
static void fbx_dispatch(unsigned i){assert(!files_ui.token&&live);dispatches++;selected=i;}
static void fb_message(const char*m){assert(m);messages++;}
#include "../../Apps/files_scene_handoff.inc"
static void reset(void){files_handler_pending=false;files_ui.token=1;current=(files_text_identity){0};strcpy(current.path,"/");strcpy(current.file_path,"/one");memset(fb_handlers,0,sizeof(fb_handlers));strcpy(fb_handlers[2].app_id,"reader");strcpy(fb_handler_path,"/one");live=pause_ok=close_ok=true;invalidated=false;dispatches=selected=messages=closes=0;}
int main(void){
 reset();assert(!files_scene_handler_ready(2)&&files_handler_pending&&!dispatches);close_ok=false;files_handler_step();assert(files_handler_pending&&!dispatches&&files_ui.token);close_ok=true;files_handler_step();assert(!files_handler_pending&&dispatches==1&&selected==2);files_handler_step();assert(dispatches==1);
 reset();assert(!files_scene_handler_ready(2));current.mount_epoch++;files_handler_step();assert(!dispatches&&messages==1);
 reset();assert(!files_scene_handler_ready(2));strcpy(fb_handlers[2].app_id,"replacement");files_handler_step();assert(!dispatches&&messages==1);
 reset();assert(!files_scene_handler_ready(2));strcpy(fb_handler_path,"/replacement");files_handler_step();assert(!dispatches&&messages==1);
 reset();assert(!files_scene_handler_ready(2));pause_ok=false;files_handler_step();assert(!dispatches&&!closes&&files_handler_pending);
 reset();assert(!files_scene_handler_ready(2));invalidated=true;files_handler_step();assert(!dispatches&&files_handler_pending&&!live);
 reset();assert(!files_scene_handler_ready(6)&&!files_handler_pending);files_ui.token=0;assert(files_scene_handler_ready(2)&&!files_handler_pending);
 /* Exercise the pending helper's direct guard in this small translation unit. */
 files_text_pending queue;files_text_pending_init(&queue,NULL,NULL,NULL,NULL,NULL,NULL);assert(!files_text_pending_request(&queue,0,"x","x",1));files_text_pending_cancel(&queue);assert(files_text_pending_step(&queue)==FILES_TEXT_RETAINED);
 puts("7 Files handoff gates PASS (endpoint doubles)");
}

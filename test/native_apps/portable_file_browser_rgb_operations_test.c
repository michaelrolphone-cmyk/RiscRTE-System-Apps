/* Real RGB565 raster/controller over the ordinary operations fixture. */
#define FILE_BROWSER_RGB_PROFILE
#define PORTABLE_FILE_BROWSER_RGB_ONLY
#include "portable_file_browser_operations_test.c"
#undef main
static unsigned pixels_changed(void){unsigned n=0;for(unsigned y=0;y<240;y++)for(unsigned x=0;x<240;x++)n+=fb_pixels[y*244+x]!=0xa5a5;return n;}
static void expect_rgb_copy(void){assert(find(&usb,"/read.txt")>=0&&usb.commits==1&&!fb_storage_index);no_handles();}
static void check_rgb_raw_controller(void){
 static const controller_step steps[]={
  {FB_LIST,100,140,NULL},{FB_ACTIONS,190,205,NULL},
  {FB_ACTIONS,190,205,NULL},{FB_ACTIONS,100,80,NULL},
  {FB_PREVIEW,25,25,NULL},{FB_ACTIONS,100,140,NULL},
  {FB_RENAME,25,25,NULL},{FB_ACTIONS,190,205,NULL},
  {FB_ACTIONS,190,205,NULL},{FB_ACTIONS,190,205,NULL},
  {FB_ACTIONS,190,205,NULL},{FB_ACTIONS,100,140,NULL},
  {FB_DESTINATION,205,25,NULL},{FB_DESTINATION,145,205,NULL},
  {FB_LIST,25,25,expect_rgb_copy}
 };
 controller_disk_setup();RUN_CONTROLLER(steps);
}
int main(void){
 init_disk(&sd);init_disk(&usb);add(&sd,"/read.txt",false,777);test_runtime_override=&runtime;
 memset(fb_pixels,0xa5,sizeof(fb_pixels));assert(app_module_init()==0);assert(fb_open()&&!fb_paper&&fb_page_rows==2);
 assert(fb_option_count()==6);select_item("/read.txt");fb_draw();assert(pixels_changed()>1000);
 /* Last actions page has one row: a blank row and the inter-row gap do nothing. */
 fb_action_page=3;fb_choice=6;fb_draw();assert(!fb_touch(100,130)&&fb_mode==FB_ACTIONS&&fb_choice==6);
 assert(!fb_touch(100,118)&&fb_mode==FB_ACTIONS&&fb_choice==6);
 /* Preview, rename keyboard Cancel, delete Cancel are visible RGB modes. */
 fb_action_page=1;fb_choice=2;fb_draw();assert(!fb_touch(100,80)&&fb_mode==FB_PREVIEW);fb_draw();assert(!fb_back()&&fb_mode==FB_ACTIONS);
 fb_action_page=1;fb_choice=2;assert(!fb_touch(100,140)&&fb_mode==FB_RENAME);fb_draw();assert(!fb_touch(25,25)&&fb_mode==FB_ACTIONS);
 fb_action_page=3;fb_choice=6;assert(!fb_touch(100,80)&&fb_mode==FB_DELETE);fb_draw();assert(!fb_touch(120,200)&&fb_mode==FB_DELETE);assert(!fb_touch(40,200)&&fb_mode==FB_ACTIONS);
 /* Actual copy destination, volume switch, complete publication and retry UI. */
 fb_action_page=2;fb_choice=4;assert(!fb_touch(100,140)&&fb_mode==FB_DESTINATION);fb_draw();
 assert(!fb_touch(205,25)&&fb_storage_index==1);fb_draw();
 assert(!fb_touch(145,205)&&fb_mode==FB_LIST&&find(&usb,"/read.txt")>=0);no_handles();
 select_item("/read.txt");fb_action_page=2;fb_choice=4;fbx_destination(false);fb_draw();
 assert(!fb_touch(30,205)&&fb_mode==FB_LIST);no_handles();
 fbx_notice("Copy publication unknown. Reload destination.");fb_draw();assert(!fb_touch(120,200)&&fb_mode==FB_ACTIONS);
 assert(portable_file_browser_close());app_module_fini();assert(!test_grants&&!test_frames&&!test_subs);
 check_rgb_raw_controller();
 puts("RGB browser ordered raw-touch app_main: preview, rename Cancel and cross-volume copy PASS");
 puts("RGB browser raster, action paging, preview, rename/delete cancellation, copy, volume selection and notices PASS");
}

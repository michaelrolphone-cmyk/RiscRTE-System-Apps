#define main legacy_browser_regressions
#include "file_browser_test.c"
#undef main
#include "../../Apps/file_browser.c"
static unsigned protected_row,protected_open_calls;
static bool protected_dir_open(const char *path){assert(!risc_protected_settings_path(path));++protected_open_calls;protected_row=0;return true;}
static bool protected_dir_next(t5_app_dirent_t *out){
 const char *names[]={"RTECFG","rtecfg. ","ordinary.txt"};
 if(protected_row==3)return false;
 memset(out,0,sizeof(*out));strcpy(out->name,names[protected_row++]);return true;
}
int main(void){
 t5_app_api_v1 test_app=app_api;test_app.dir_open=protected_dir_open;test_app.dir_next=protected_dir_next;
 app=&test_app;browser=&browser_api;storage=&storage_api;file_open=&file_open_api;system_ui=&system_ui_api;
 strcpy(base_path,"/");assert(load_sd_files()&&entry_count==1&&!strcmp(entries[0].name,"ordinary.txt"));
 strcpy(base_path,"/rTeCfG. ");assert(!load_sd_files()&&protected_open_calls==1);
 strcpy(base_path,"/");entry_count=1;selected_index=0;strcpy(entries[0].name,"RTECFG");entries[0].is_directory=true;
 assert(!open_selected()&&!request_delete());assert(!valid_entry_name("rtecfg.")&&!valid_entry_name("RTECFG "));
 assert(!copy_sd_to_usb("/sd/RTECFG/index","/copy"));assert(!copy_usb_to_sd("/RTECFG/index","/sd/copy"));
 assert(!open_with_handler("rtecfg",NULL));assert(list_picker_directories(false,"/RTECFG",0)==0);
 puts("Legacy Files reserved settings paths PASS");
 return 0;
}

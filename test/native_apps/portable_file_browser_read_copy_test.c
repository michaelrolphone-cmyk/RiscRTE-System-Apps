/* Read/copy-only is a generic optional profile; no app/namespace special case. */
#define PORTABLE_FILE_BROWSER_READ_COPY_ONLY
#define PORTABLE_FILE_BROWSER_RGB_ONLY
#define PORTABLE_FILE_BROWSER_SECONDARY_CAPABILITY "storage.app-data.export"
#define PORTABLE_FILE_BROWSER_SECONDARY_INSTANCE 0
#define APP_DATA_EXPORT_TEST_ENTRY read_copy_existing_fixture_main
#include "portable_file_browser_appdata_export_test.c"
static bool should_not_remove(void*c,const char*p){(void)c;(void)p;assert(false);return false;}
static bool should_not_rename(void*c,const char*a,const char*b){(void)c;(void)a;(void)b;assert(false);return false;}
int main(void){
 assert(FBX_ACTION_COUNT==2&&!strcmp(fbx_labels[0],"Preview")&&!strcmp(fbx_labels[1],"Copy"));
 /* Existing real copy/cancel fixture exercises all copy outcomes unchanged. */
 int result=read_copy_existing_fixture_main();assert(result==0);
 risc_storage_volume_api_v1_ext table={0};table.base=test_volume;table.base.struct_size=sizeof(table);
 table.base.label=test_label;table.base.dir_open=test_diropen;table.base.dir_next=test_dirnext;table.base.dir_close=test_dirclose;table.base.last_error=test_error_get;
 assert(fb_api_valid(&table.base));
 table.base.remove=should_not_remove;assert(!fb_api_valid(&table.base));table.base.remove=NULL;
 table.rename=should_not_rename;assert(!fb_api_valid(&table.base));table.rename=NULL;
 table.mkdir=should_not_remove;assert(!fb_api_valid(&table.base));table.mkdir=NULL;
 assert(fb_api_valid(&table.base));table.base.struct_size=8;assert(!fb_api_valid(&table.base));
 /* Execute the selected profile through the real RGB raster and touch rows. */
 test_volume=table.base;test_volume.struct_size=sizeof(test_volume);test_volume.file_open_write=export_open;test_volume.file_write=export_write;
 test_grants=test_frames=test_subs=0;test_entries=2;test_length=577;test_script=0;fb_grant.api=NULL;fb_volume=NULL;
 assert(app_module_init()==0);assert(fb_open()&&!fb_paper&&fb_option_count()==6);
 assert(!fb_touch(100,80)&&fb_mode==FB_ACTIONS);fb_draw();
 assert(!fb_touch(100,80)&&fb_mode==FB_PREVIEW);fb_draw();assert(!fb_back()&&fb_mode==FB_ACTIONS);
 assert(!fb_touch(100,118)&&fb_mode==FB_ACTIONS); /* Inter-row gap is inert. */
 assert(!fb_touch(100,140)&&fb_mode==FB_DESTINATION);fb_draw();assert(!fb_touch(30,205)&&fb_mode==FB_LIST);
 assert(portable_file_browser_close());app_module_fini();assert(!test_grants&&!test_frames&&!test_subs);
 puts("Read-copy-only RGB preview, destination/cancel and bounded menu PASS");
 puts("Read-copy-only profile preserves copy failures and refuses incompatible mutation tables PASS");return 0;
}

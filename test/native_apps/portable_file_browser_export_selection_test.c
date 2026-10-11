/* Actual selector keeps the installed-files view and chooses the explicit
 * app-data export capability at its permitted unique instance zero. */
#define PORTABLE_FILE_BROWSER_SECONDARY_CAPABILITY "storage.app-data.export"
#define PORTABLE_FILE_BROWSER_SECONDARY_INSTANCE 0
#define main legacy_file_browser_selection_fixture_main
#include "portable_file_browser_test.c"
#undef main
static risc_storage_volume_api_v1 saved_volume;
static unsigned requested,released,installed_requested;
static bool deny_export;
static bool selection_acquire(const char*cap,uint32_t api,uint64_t instance,risc_runtime_capability_v1*g){
 assert(api==1&&instance==0);
 if(!strcmp(cap,"storage.installed-files")){++installed_requested;g->api=&test_volume;return true;}
 assert(!strcmp(cap,"storage.app-data.export"));++requested;
 if(deny_export)return false;
 g->api=&saved_volume;return true;
}
static bool selection_release(risc_runtime_capability_v1*g){assert(g->api==&test_volume||g->api==&saved_volume);++released;g->api=NULL;return true;}
int main(void){
 test_volume=(risc_storage_volume_api_v1){1,sizeof(test_volume),NULL,test_ready,test_ready,test_label,test_stat,test_diropen,test_dirnext,test_dirclose,test_open,test_read,NULL,NULL,test_close,NULL,test_error_get};
 saved_volume=test_volume;static risc_runtime_api_v1 runtime;runtime=test_runtime;runtime.acquire=selection_acquire;runtime.release=selection_release;fb_runtime=&runtime;
 fb_manage=false;assert(fb_option_count()==6);
 fb_volume=&test_volume;fb_grant=(risc_runtime_capability_v1){.struct_size=sizeof(fb_grant),.api=&test_volume};fb_storage_index=0;
 deny_export=true;assert(!fbx_swap()&&fb_volume==&test_volume&&fb_storage_index==0&&!fb_other_grant.api);
 deny_export=false;assert(fbx_swap()&&fb_volume==&saved_volume&&fb_storage_index==1&&requested==2);
 assert(fbx_swap()&&fb_volume==&test_volume&&fb_storage_index==0&&requested==2);
 assert(portable_file_browser_close()&&released==2);
 /* Closing releases both selectors but preserves the selected index. Actual
  * refresh/acquisition must use that selector, rather than storage.volume. */
 assert(!fb_volume&&!fb_grant.api&&!fb_other_grant.api);
 assert(fb_acquire()&&fb_volume==&test_volume&&installed_requested==1);
 assert(portable_file_browser_close()&&released==3);
 fb_storage_index=1;deny_export=true;
 assert(!fb_acquire()&&!fb_volume&&!fb_grant.api&&requested==3);
 deny_export=false;
 assert(fb_acquire()&&fb_volume==&saved_volume&&requested==4);
 assert(fb_acquire()&&requested==4); /* No duplicate grant while selected. */
 assert(portable_file_browser_close()&&released==4&&fb_storage_index==1);
 assert(fb_acquire()&&fb_volume==&saved_volume&&requested==5);
 assert(portable_file_browser_close()&&released==5);
 fb_storage_index=0;
 assert(fb_acquire()&&fb_volume==&test_volume&&installed_requested==2);
 assert(portable_file_browser_close()&&released==6);
 puts("Actual browser swap, close, refresh and explicit app-data reacquisition PASS");
}

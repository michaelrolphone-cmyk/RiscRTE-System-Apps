/* Actual portable browser/controller and adapter, with the exact tagged export
 * SDK and a transactional provider fixture. No runtime or product admission. */
#define main legacy_file_browser_fixture_main
#include "portable_file_browser_test.c"
#undef main

/* An exclusive writer stages only transient bytes; COMMIT_UNKNOWN can publish
 * before returning false. Cancelling it must never remove the committed file. */
typedef struct {
 risc_app_data_export_v1 api;
 bool live,published,attempted,fail_cancel,override_status,publish_unknown;
 int32_t result,last_status,reported_status;
 unsigned opens,commit_calls,replacements,cancel_calls,status_calls,releases,revision_calls;
 size_t staged_size,published_size;
 unsigned char stage[1024],bytes[1024];
 char path[FB_PATH_CAP];
} export_disk;
static export_disk disk;
static risc_storage_volume_api_v1 *export_volume(void){return &disk.api.volume.terminal.power.volume.base;}
static bool export_stat(void*c,const char*p,uint64_t*s,bool*d){export_disk*x=c;*d=!strcmp(p,"/");*s=*d?0:x->published_size;return *d || (x->published&&!strcmp(p,x->path));}
static uint32_t export_open(void*c,const char*p){export_disk*x=c;x->opens++;if(x->live||(x->published&&!strcmp(p,x->path)))return 0;x->live=true;x->attempted=false;x->last_status=0;x->staged_size=0;strcpy(x->path,p);return 7;}
static size_t export_write(void*c,uint32_t h,const void*data,size_t n){export_disk*x=c;assert(h==7&&x->live&&!x->attempted);if(n>11)n=11;assert(x->staged_size+n<=sizeof(x->stage));memcpy(x->stage+x->staged_size,data,n);x->staged_size+=n;return n;}
static bool export_close(void*c,uint32_t h,bool commit){
 export_disk*x=c;assert(h==7&&x->live);
 if(!commit){x->cancel_calls++;if(x->fail_cancel)return false;x->live=false;x->staged_size=0;return true;}
 x->commit_calls++;if(x->attempted)return false;
 x->attempted=true;x->replacements++;x->last_status=x->result;
 if(!x->result || (x->result==RISC_APP_DATA_COMMIT_UNKNOWN&&x->publish_unknown)){
  memcpy(x->bytes,x->stage,x->staged_size);x->published_size=x->staged_size;x->published=true;
 }
 if(x->result)return false;
 x->live=false;x->staged_size=0;return true;
}
static int32_t export_status(void*c,uint32_t h){export_disk*x=c;x->status_calls++;if(!x->live||h!=7)return RISC_APP_DATA_CONTEXT;return x->override_status?x->reported_status:x->last_status?x->last_status:RISC_APP_DATA_EXPORT_WRITE_PENDING;}
static int32_t revision_stat(void*c,const char*p,uint32_t*n,uint64_t*r){(void)p;(void)n;(void)r;((export_disk*)c)->revision_calls++;return RISC_APP_DATA_CONTEXT;}
static int32_t revision_read(void*c,const char*p,uint64_t r,void*b,uint32_t n,uint32_t*out,uint64_t*next){(void)b;(void)r;(void)n;return revision_stat(c,p,out,next);}
static int32_t revision_replace(void*c,const char*p,uint64_t r,const void*b,uint32_t n){(void)r;(void)b;(void)n;return revision_stat(c,p,NULL,NULL);}
static bool export_release(risc_runtime_capability_v1*g){assert(g->api==export_volume()&&!disk.live);disk.releases++;g->api=NULL;return true;}
static bool export_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){assert(!strcmp(n,PORTABLE_FILE_BROWSER_CAPABILITY)&&v==1&&!id);g->api=export_volume();return true;}
static const risc_runtime_api_v1 export_runtime={1,sizeof(export_runtime),test_health,test_yield,test_diag,test_launch,export_acquire,export_release};
static void setup(void){
 assert(portable_file_browser_safe()&&!fb_grant.api);memset(&disk,0,sizeof(disk));
 *export_volume()=(risc_storage_volume_api_v1){.api_version=1,.struct_size=sizeof(disk.api),.context=&disk,.refresh=test_ready,.ready=test_ready,.label=test_label,.stat=export_stat,.dir_open=test_diropen,.dir_next=test_dirnext,.dir_close=test_dirclose,.file_open_read=test_open,.file_read=test_read,.last_error=test_error_get,.file_open_write=export_open,.file_write=export_write,.file_close=export_close};
 disk.api.export_tag=RISC_APP_DATA_EXPORT_TAG;disk.api.export_version=RISC_APP_DATA_EXPORT_API_V1;disk.api.write_status=export_status;disk.api.stat_revision=revision_stat;disk.api.read_revision=revision_read;disk.api.replace_revision=revision_replace;
 test_volume=(risc_storage_volume_api_v1){.api_version=1,.struct_size=sizeof(test_volume),.refresh=test_ready,.ready=test_ready,.stat=test_stat,.file_open_read=test_open,.file_read=test_read,.file_close=test_close};
 fb_runtime=&export_runtime;fb_grant=(risc_runtime_capability_v1){.struct_size=sizeof(fb_grant),.api=export_volume()};fb_volume=export_volume();fb_mode=FB_DESTINATION;fb_storage_index=fb_source_index=0;fb_publication_unknown=false;
#ifndef PORTABLE_FILE_BROWSER_READ_COPY_ONLY
 fb_moving=false;
#endif
 strcpy(fb_source_base,"/");strcpy(fb_path,"/");fb_source_query[0]=0;fb_query[0]=0;test_fail_close=false;test_fail_read=false;test_entries=0;
}
static bool copy(void){return fbx_copy(&test_volume,"/source.txt",export_volume(),"/copy.txt");}
static void expected_bytes(void){assert(disk.published&&disk.published_size==test_length);for(unsigned i=0;i<test_length;i++)assert(disk.bytes[i]=='A'+i%26);}
static void closed(void){assert(!disk.live&&portable_file_browser_safe());assert(portable_file_browser_close());assert(disk.releases==1&&!fb_grant.api&&!disk.revision_calls);}
static void expect_retained(int32_t reported){
 setup();disk.result=RISC_APP_DATA_NO_SPACE;disk.override_status=true;disk.reported_status=reported;
 assert(!copy()&&!portable_file_browser_safe()&&disk.live&&!disk.cancel_calls);
 assert(!portable_file_browser_close()&&fb_grant.api&&!disk.releases&&!disk.cancel_calls&&disk.replacements==1);
 /* Remove the test-only status mask to exercise known-failure cleanup. This
  * does not model recovery from a real RETAINED state. Back cancels directly,
  * so even the commit callback is not called a third time. */
 unsigned calls=disk.commit_calls;disk.override_status=false;
 assert(portable_file_browser_close()&&disk.commit_calls==calls&&disk.cancel_calls==1&&disk.replacements==1&&disk.releases==1);
}
#ifndef APP_DATA_EXPORT_TEST_ENTRY
#define APP_DATA_EXPORT_TEST_ENTRY main
#endif
int APP_DATA_EXPORT_TEST_ENTRY(void){
 /* Successful exclusive copy and collision: no overwrite or second open. */
 setup();assert(risc_app_data_export(export_volume())==&disk.api);assert(!risc_storage_volume_power_commit(export_volume())&&!risc_storage_volume_sleep(export_volume()));assert(!risc_storage_volume_power(export_volume())->prepare_power_down&&!risc_storage_volume_power(export_volume())->cancel_power_down);assert(copy());expected_bytes();assert(disk.replacements==1&&disk.commit_calls==1&&!disk.cancel_calls);assert(!copy()&&strstr(fb_status,"exists")&&disk.opens==1);closed();
 setup();test_length=0;assert(copy()&&disk.published&&!disk.published_size);closed();test_length=577;
 /* Failed source close still aborts the export destination before publication. */
 setup();test_fail_close=true;assert(!copy()&&!disk.live&&!disk.published&&!disk.replacements&&disk.cancel_calls==1&&!portable_file_browser_safe());assert(!portable_file_browser_close()&&!disk.releases);test_fail_close=false;assert(portable_file_browser_close()&&portable_file_browser_safe()&&disk.releases==1);
 /* Failure cleanup must not turn copy failure into success. A fresh user copy
  * can succeed after NO_SPACE clears; cleanup never replays replacement. */
 setup();disk.result=RISC_APP_DATA_NO_SPACE;assert(!copy());assert(!disk.live&&!disk.published&&portable_file_browser_safe());assert(disk.replacements==1&&disk.commit_calls==1&&disk.cancel_calls==1&&strstr(fb_status,"Copy failed"));assert(fbx_close_owned()&&disk.replacements==1);disk.result=0;assert(copy());expected_bytes();assert(disk.replacements==2&&disk.opens==2);closed();
 /* Both possible publication outcomes remain unknown to the browser. */
 for(unsigned published=0;published<2;published++){
  setup();disk.result=RISC_APP_DATA_COMMIT_UNKNOWN;disk.publish_unknown=published;assert(!copy());assert(!disk.live&&portable_file_browser_safe()&&fb_publication_unknown);assert(strstr(fb_status,"publication unknown")&&strstr(fb_status,"Reload")&&!strstr(fb_status,"aborted"));assert(disk.published==(bool)published&&disk.replacements==1&&disk.commit_calls==1&&disk.cancel_calls==1);if(published)expected_bytes();fbx_cancel();assert(fb_mode==FB_LIST&&strstr(fb_status,"publication unknown")&&strstr(fb_status,"Reload")&&!strstr(fb_status,"cancelled"));closed();assert(disk.replacements==1);
 }
 /* Failed cancellation retains the grant, remembers cancellation intent, and
  * repeats only cleanup. A later successful cleanup cannot claim publication. */
 setup();disk.result=RISC_APP_DATA_COMMIT_UNKNOWN;disk.publish_unknown=true;disk.fail_cancel=true;assert(!copy()&&!portable_file_browser_safe());assert(fb_owned[0].handle==7&&!fb_owned[0].commit&&strstr(fb_status,"publication unknown"));assert(!fb_ready()&&strstr(fb_status,"publication unknown"));assert(!portable_file_browser_close()&&disk.live&&fb_grant.api&&!disk.releases&&disk.commit_calls==1&&disk.replacements==1);assert(!fb_back()&&strstr(fb_status,"publication unknown")&&!strstr(fb_status,"aborted"));disk.fail_cancel=false;
 assert(!fb_back()&&fb_mode==FB_LIST&&strstr(fb_status,"publication unknown")&&strstr(fb_status,"Reload"));expected_bytes();assert(!disk.live&&fb_grant.api&&disk.releases==1&&disk.commit_calls==1&&disk.replacements==1);assert(portable_file_browser_close()&&disk.releases==2);
 /* RETAINED, nonwriter CONTEXT, PENDING and unrecognized status are never a
  * reason to cancel or release. Existing checked-close retries stay intact. */
 expect_retained(RISC_APP_DATA_RETAINED);expect_retained(RISC_APP_DATA_CONTEXT);expect_retained(RISC_APP_DATA_EXPORT_WRITE_PENDING);expect_retained(-123);expect_retained(RISC_APP_DATA_OK);
 /* Exact tag/version/full-size/write_status guards. These other volumes keep
  * the legacy commit intent even if their unused status member says NO_SPACE. */
 for(unsigned guard=0;guard<6;guard++){
  setup();disk.result=RISC_APP_DATA_NO_SPACE;
  if(guard==0)disk.api.export_tag^=1;
  if(guard==1)disk.api.export_version++;
  if(guard==2)export_volume()->struct_size=sizeof(disk.api)-1;
  if(guard==3)disk.api.write_status=NULL;
  if(guard==4)export_volume()->struct_size=sizeof(risc_storage_volume_api_v1);
  if(guard==5)export_volume()->api_version=2;
  assert(!copy()&&disk.live&&!portable_file_browser_safe()&&!disk.cancel_calls&&!disk.status_calls&&fb_owned[0].commit);
  assert(!portable_file_browser_close()&&!disk.cancel_calls&&!disk.releases&&disk.commit_calls==2&&disk.replacements==1);
  /* Restore the exact contract solely to clean up the test fixture. */
  disk.api.export_tag=RISC_APP_DATA_EXPORT_TAG;disk.api.export_version=1;export_volume()->api_version=1;export_volume()->struct_size=sizeof(disk.api);disk.api.write_status=export_status;
  assert(portable_file_browser_close()&&!disk.live&&disk.replacements==1&&disk.commit_calls==2&&disk.releases==1);
 }
 puts("App-data export browser: exclusive copy, NO_SPACE cleanup/fresh retry, COMMIT_UNKNOWN reload/no replay, failed cancellation retention, exact extension/status guards passed");
 return 0;
}

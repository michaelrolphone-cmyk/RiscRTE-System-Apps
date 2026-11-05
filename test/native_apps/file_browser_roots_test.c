/* Reuse the real controller and fault-injecting volumes; no alternate I/O. */
#define PORTABLE_FILE_BROWSER_TERTIARY_CAPABILITY "storage.removable"
#define PORTABLE_FILE_BROWSER_TERTIARY_INSTANCE 33
#define FILES_OPERATIONS_MAIN legacy_operations_main
#include "portable_file_browser_operations_test.c"
static disk third;
static bool deny_third,deny_second;
static unsigned third_acquires;
static bool root_acquire(const char*n,uint32_t version,uint64_t instance,risc_runtime_capability_v1*g){
 if(!strcmp(n,"storage.removable")){assert(version==1&&instance==33);third_acquires++;if(deny_third)return false;g->api=&third.api;test_grants++;return true;}
 if(instance==22&&deny_second)return false;
 return acquire(n,version,instance,g);
}
static bool root_release(risc_runtime_capability_v1*g){
 if(g->api==&third.api){assert(!third.open_dirs);for(unsigned i=0;i<8;i++)assert(!third.handles[i].used);}
 return release(g);
}
int main(void){
 controller_disk_setup();init_disk(&third);
 static risc_runtime_api_v1 rt;rt=runtime;rt.acquire=root_acquire;rt.release=root_release;
 test_runtime_override=&rt;assert(app_module_init()==0);unsigned base_grants=test_grants;assert(fb_open());
 assert(FB_LOCAL_ROOT_COUNT==3&&FB_REMOTE_ROOT==3);
 deny_second=true;assert(fbx_swap()&&fb_storage_index==2&&third_acquires==1);
 assert(fbx_swap()&&fb_storage_index==0);
 deny_third=true;/* An acquired root remains owned; denial cannot revoke its grant. */
 assert(fbx_swap()&&fb_storage_index==2&&third_acquires==1);
 assert(portable_file_browser_close());assert(test_grants==base_grants);
 fb_storage_index=0;assert(fb_acquire());
 assert(!fbx_swap()&&fb_storage_index==0&&third_acquires==2);
 deny_third=deny_second=false;assert(fbx_swap()&&fb_storage_index==1);
 third.offline=true;assert(fbx_swap()&&fb_storage_index==2);assert(!fb_ready());
 third.offline=false;assert(fb_ready());
 /* Copy source survives selection of both other roots; cancel restores the
  * exact source index, not one toggle from the current destination. */
 for(unsigned from=0;from<3;from++)for(unsigned to=0;to<3;to++)if(from!=to){
  assert(fbx_select_local(from));disk*d=(disk*)fb_volume->context;
  char name[32];snprintf(name,sizeof(name),"/copy-%u-%u.txt",from,to);add(d,name,false,517);
  select_item(name);fbx_destination(false);assert(fb_source_index==from);
  assert(fbx_select_local(to));strcpy(fb_path,"/");fbx_commit_destination();
  assert(fb_storage_index==from&&fb_mode==FB_LIST);
  disk*destination=(disk*)fb_local_volume(to)->context;int n=find(destination,name);assert(n>=0&&destination->nodes[n].size==517);
 }
 assert(fbx_select_local(2));select_item("/copy-2-0.txt");fbx_destination(false);assert(fbx_select_local(0));fbx_cancel();assert(fb_storage_index==2);
 release_target=&third.api;assert(!portable_file_browser_close());assert(fb_grant.api==&third.api);
 release_target=NULL;assert(portable_file_browser_close());app_module_fini();assert(!test_grants&&!test_frames&&!test_subs);
 puts("Generic roots: missing capability, three-way selection/copy, exact cancel return, absent/reinserted media, retained release PASS");
}

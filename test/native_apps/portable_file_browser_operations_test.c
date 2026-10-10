#define FILE_BROWSER_PAPER_PROFILE
#define PORTABLE_DISPLAY_ROTATION 90
#define PORTABLE_FILE_BROWSER_CAPABILITY "storage.volume"
#define PORTABLE_FILE_BROWSER_INSTANCE 11
#define PORTABLE_FILE_BROWSER_SECONDARY_INSTANCE 22
#define PORTABLE_FILE_BROWSER_HANDLERS
#define FILE_BROWSER_RETURN_APP "springboard.elf"
#define main legacy_file_browser_fixture_main
#include "portable_file_browser_test.c"
#undef main

typedef struct {char path[512];bool used,dir;unsigned size;unsigned char data[8192];} node;
typedef struct {unsigned node,offset;bool used,writing;} handle;
typedef struct {risc_storage_volume_api_v1_ext api;node nodes[32];handle handles[8];char dir[512];unsigned cursor,open_dirs;bool offline,fail_open_read,fail_open_write,fail_read,fail_write,fail_close,fail_dir_close,fail_listing,fail_rename,fail_remove,grow;unsigned removals,renames,commits,aborts,read_calls,write_calls,read_opens,write_opens,close_calls,rename_calls,remove_calls,drop_on_read,drop_on_write;} disk;
static disk sd,usb;
static unsigned handler_calls,handler_count=8;static bool handler_missing,handler_invalid,handler_request_fail,release_fail;
static const void *release_target;
static int32_t handler_result_error;static bool handler_result_pending;
static char requested_path[512],requested_app[64];
static int find(disk*d,const char*p){for(unsigned i=0;i<32;i++)if(d->nodes[i].used&&!strcmp(p,d->nodes[i].path))return (int)i;return -1;}
static node *add(disk*d,const char*p,bool dir,unsigned size){assert(find(d,p)<0);for(unsigned i=0;i<32;i++)if(!d->nodes[i].used){node*n=&d->nodes[i];memset(n,0,sizeof(*n));n->used=true;n->dir=dir;n->size=size;strcpy(n->path,p);for(unsigned j=0;j<size;j++)n->data[j]=(unsigned char)(j%251);return n;}assert(false);return NULL;}
static bool ready(void*c){return !((disk*)c)->offline;}
static bool disk_label(void*c,char*out,size_t n){snprintf(out,n,"%s",c==&sd?"SD card":"USB Storage");return true;}
static bool stat_file(void*c,const char*p,uint64_t*s,bool*dir){disk*d=c;int i=find(d,p);if(i<0||d->offline)return false;*s=d->nodes[i].size+(d->grow?1:0);*dir=d->nodes[i].dir;return true;}
static uint32_t dir_open(void*c,const char*p){disk*d=c;int i=find(d,p);if(i<0||!d->nodes[i].dir||d->offline)return 0;assert(!d->open_dirs);d->open_dirs++;d->cursor=0;strcpy(d->dir,p);return 99;}
static bool dir_next(void*c,uint32_t h,risc_storage_dirent_v1*out){disk*d=c;assert(h==99&&d->open_dirs);if(d->fail_listing||d->offline)return false;while(d->cursor<32){node*n=&d->nodes[d->cursor++];if(!n->used||!strcmp(n->path,d->dir))continue;size_t len=strlen(d->dir);if(strncmp(n->path,d->dir,len))continue;const char*p=n->path+len;if(len>1){if(*p!='/')continue;p++;}if(strchr(p,'/'))continue;memset(out,0,sizeof(*out));snprintf(out->name,sizeof(out->name),"%s",p);out->is_directory=n->dir;out->size=n->size;return true;}return false;}
static bool dir_checked(void*c,uint32_t h){disk*d=c;assert(h==99&&d->open_dirs);if(d->fail_dir_close)return false;d->open_dirs--;return true;}
static void dir_close(void*c,uint32_t h){assert(dir_checked(c,h));}
static uint32_t handle_error(void*c,uint32_t h,bool directory){(void)h;disk*d=c;return directory&&d->fail_listing?1:0;}
static uint32_t open_read(void*c,const char*p,uint64_t*s){disk*d=c;int n=find(d,p);if(n<0||d->nodes[n].dir||d->offline||d->fail_open_read)return 0;d->read_opens++;for(unsigned i=0;i<8;i++)if(!d->handles[i].used){d->handles[i]=(handle){(unsigned)n,0,true,false};*s=d->nodes[n].size;return i+1;}assert(false);return 0;}
static uint32_t open_write(void*c,const char*p){disk*d=c;if(find(d,p)>=0||d->offline||d->fail_open_write)return 0;d->write_opens++;node*n=add(d,p,false,0);for(unsigned i=0;i<8;i++)if(!d->handles[i].used){d->handles[i]=(handle){(unsigned)(n-d->nodes),0,true,true};return i+1;}assert(false);return 0;}
static size_t read_file(void*c,uint32_t h,void*out,size_t n){disk*d=c;handle*f=&d->handles[h-1];assert(f->used&&!f->writing);if(++d->read_calls==d->drop_on_read)d->offline=true;if(d->fail_read||d->offline)return 0;node*x=&d->nodes[f->node];if(n>311)n=311;if(n>x->size-f->offset)n=x->size-f->offset;memcpy(out,x->data+f->offset,n);f->offset+=(unsigned)n;return n;}
static size_t write_file(void*c,uint32_t h,const void*in,size_t n){disk*d=c;handle*f=&d->handles[h-1];assert(f->used&&f->writing);if(++d->write_calls==d->drop_on_write)d->offline=true;if(d->fail_write||d->offline)return 0;if(n>83)n=83;node*x=&d->nodes[f->node];assert(n<=sizeof(x->data)-f->offset);memcpy(x->data+f->offset,in,n);f->offset+=(unsigned)n;x->size=f->offset;return n;}
static bool close_file(void*c,uint32_t h,bool commit){disk*d=c;handle*f=&d->handles[h-1];assert(f->used);d->close_calls++;if(d->fail_close)return false;if(f->writing){if(commit)d->commits++;else {d->aborts++;d->nodes[f->node].used=false;}}memset(f,0,sizeof(*f));return true;}
static bool remove_file(void*c,const char*p){disk*d=c;d->remove_calls++;int i=find(d,p);if(i<0||d->fail_remove||d->offline)return false;for(unsigned j=0;j<32;j++)if(j!=(unsigned)i&&d->nodes[j].used&&fbx_same_or_child(d->nodes[j].path,p))return false;d->nodes[i].used=false;d->removals++;return true;}
static bool rename_file(void*c,const char*from,const char*to){disk*d=c;d->rename_calls++;int i=find(d,from);if(i<0||find(d,to)>=0||d->fail_rename||d->offline)return false;for(unsigned j=0;j<32;j++)if(d->nodes[j].used&&fbx_same_or_child(d->nodes[j].path,from)){char new_path[512];snprintf(new_path,sizeof(new_path),"%s%s",to,d->nodes[j].path+strlen(from));strcpy(d->nodes[j].path,new_path);}d->renames++;return true;}
static bool error_text(void*c,char*out,size_t n){disk*d=c;snprintf(out,n,"%s",d->fail_listing?"Directory read failed":"");return true;}
static void init_disk(disk*d){memset(d,0,sizeof(*d));d->api.base=(risc_storage_volume_api_v1){1,sizeof(d->api),d,ready,ready,disk_label,stat_file,dir_open,dir_next,dir_close,open_read,read_file,open_write,write_file,close_file,remove_file,error_text};d->api.dir_close_checked=dir_checked;d->api.handle_error=handle_error;d->api.rename=rename_file;add(d,"/",true,0);add(d,"/Books",true,0);}
static uint32_t count_handlers(const char*p){assert(!strcmp(p,"/sd/read.txt"));return handler_count;}
static bool get_handler(const char*p,uint32_t i,t5_file_handler_t*out){assert(!strcmp(p,"/sd/read.txt")&&i<handler_count);if(handler_invalid){memset(out,1,sizeof(*out));return true;}*out=(t5_file_handler_t){0};snprintf(out->app_id,sizeof(out->app_id),"reader%u",i);snprintf(out->display_name,sizeof(out->display_name),"Reader %u",i);return true;}
static bool request_handler(const char*p,const char*app,uint64_t cookie){assert(cookie==UINT64_C(0x4642524f57534552));handler_calls++;strcpy(requested_path,p);strcpy(requested_app,app);return !handler_request_fail;}
static bool take_result(int32_t*error,uint64_t*cookie){if(!handler_result_pending)return false;*error=handler_result_error;*cookie=UINT64_C(0x4642524f57534552);handler_result_pending=false;return true;}
static t5_file_open_api_v1 handlers={.api_version=1,.struct_size=sizeof(handlers),.handler_count=count_handlers,.handler_get=get_handler,.open_request=request_handler,.open_take_result=take_result};
static bool acquire(const char*n,uint32_t version,uint64_t instance,risc_runtime_capability_v1*g){assert(version==1);if(!strcmp(n,"storage.volume")){assert(instance==11||instance==22);g->api=instance==11?&sd.api:&usb.api;}else if(!strcmp(n,"file.open")){assert(!instance);if(handler_missing)return false;g->api=&handlers;}else return test_acquire(n,version,instance,g);test_grants++;return true;}
static bool release(risc_runtime_capability_v1*g){disk*d=g->api==&sd.api?&sd:g->api==&usb.api?&usb:NULL;if(d){assert(!d->open_dirs);for(unsigned i=0;i<8;i++)assert(!d->handles[i].used);}if(release_fail||g->api==release_target)return false;return test_release(g);}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.health=test_health,.yield_ms=test_yield,.diagnostic=test_diag,.request_launch=test_launch,.acquire=acquire,.release=release};
static void select_item(const char*p){strcpy(fb_file_path,p);const char*n=strrchr(p,'/');strcpy(fb_selected.name,n+1);uint64_t size;bool dir;assert(fb_volume->stat(fb_volume->context,p,&size,&dir));fb_selected.size=size;fb_selected.is_directory=dir;strcpy(fb_path,p);assert(fb_parent(fb_path));fb_mode=FB_ACTIONS;fb_choice=fb_action_page=0;}
static void frame(const char *name){fb_dirty=true;fb_draw();const char*dir=getenv("FILE_BROWSER_PAPER_FRAMES");if(!dir)return;char p[512];snprintf(p,sizeof(p),"%s/%s.pbm",dir,name);FILE*f=fopen(p,"wb");assert(f);fprintf(f,"P4\n800 480\n");assert(fwrite(fb_pixels,1,sizeof(fb_pixels),f)==sizeof(fb_pixels));fclose(f);}
static void no_handles(void){for(unsigned i=0;i<8;i++)assert(!sd.handles[i].used&&!usb.handles[i].used);assert(!sd.open_dirs&&!usb.open_dirs&&portable_file_browser_safe());}
static void deletion_cancel_script(int*x,int*y){
 assert(test_steps<80);
 if(test_steps==5){*x=420;*y=420;} /* read.txt row actions */
 if(test_steps>=11&&test_steps<=41&&(test_steps-11)%6==0){*x=400;*y=730;}
 if(test_steps==47){*x=100;*y=150;} /* Delete */
 if(test_steps==53){*x=100;*y=730;} /* Cancel */
 if(test_steps==59||test_steps==65){*x=75;*y=730;} /* Back and return */
}
/* Feed ordered raw contact events through the production adapter and app_main.
 * Checks run between taps, after release, so state changes must come from UI. */
typedef struct { unsigned mode; int x,y; void (*check)(void); } controller_step;
static const controller_step *controller_steps;
static unsigned controller_step_count,controller_step_index;
static void controller_touch(int *x,int *y){
 assert(test_steps<300);
 if(test_steps<5||(test_steps-5)%6)return;
 assert(controller_step_index<controller_step_count);
 const controller_step *step=&controller_steps[controller_step_index++];
 assert(fb_mode==step->mode);
 if(step->check)step->check();
 *x=step->x;*y=step->y;
}
static void controller_disk_setup(void){
 init_disk(&sd);init_disk(&usb);add(&sd,"/read.txt",false,5000);
 handler_missing=handler_invalid=handler_request_fail=handler_result_pending=false;
 release_fail=false;release_target=NULL;test_script=0;
}
static void run_controller(const controller_step *steps,unsigned count){
 unsigned launches=test_launches;
 controller_steps=steps;controller_step_count=count;controller_step_index=0;
 test_polls=0;test_seen_modes=0;test_runtime_override=&runtime;test_touch_script=controller_touch;
 assert(app_module_init()==0);app_main();app_module_fini();
 test_touch_script=NULL;test_runtime_override=NULL;
 assert(controller_step_index==count&&test_launches==launches+1);
 assert(!test_grants&&!test_frames&&!test_subs);no_handles();
}
#define RUN_CONTROLLER(steps) run_controller(steps,sizeof(steps)/sizeof((steps)[0]))
static void expect_unavailable(void){assert(!fb_loaded&&!fb_count&&strstr(fb_status,"unavailable"));no_handles();}
static void reconnect_sd(void){sd.offline=false;}
static void expect_loaded(void){assert(fb_loaded&&fb_count==2&&fb_manage&&!fb_storage_index);no_handles();}
static void expect_listing_error(void){assert(!fb_loaded&&!fb_count&&strstr(fb_status,"Folder read failed"));no_handles();sd.fail_listing=false;}
static void remove_sd(void){sd.offline=true;}
static void expect_selection_removed(void){assert(strstr(fb_status,"unavailable")&&!sd.read_opens&&!sd.write_opens&&!sd.renames&&!sd.removals);no_handles();sd.offline=false;}
static void check_absent_removed_and_listing(void){
 static const controller_step absent[]={
  {FB_LIST,420,50,expect_unavailable},{FB_OPTIONS,100,244,reconnect_sd},
  {FB_LIST,75,730,expect_loaded}
 };
 controller_disk_setup();sd.offline=true;RUN_CONTROLLER(absent);
 static const controller_step listing[]={
  {FB_LIST,420,50,expect_listing_error},{FB_OPTIONS,100,244,NULL},
  {FB_LIST,75,730,expect_loaded}
 };
 controller_disk_setup();sd.fail_listing=true;RUN_CONTROLLER(listing);
 static const controller_step removed[]={
  {FB_LIST,420,244,expect_loaded},{FB_ACTIONS,100,332,remove_sd},
  {FB_NOTICE,75,730,expect_selection_removed},{FB_LIST,420,50,NULL},
  {FB_OPTIONS,100,244,NULL},{FB_LIST,75,730,expect_loaded}
 };
 controller_disk_setup();RUN_CONTROLLER(removed);
}
static void fail_preview(void){sd.fail_read=sd.fail_close=true;}
static void expect_preview_owned(void){
 assert(fb_retained_file&&!portable_file_browser_safe()&&!fb_preview_count);
 assert(fb_grant.api==&sd.api&&sd.handles[fb_retained_file-1].used);
 assert(sd.read_calls==1&&sd.read_opens==1&&sd.close_calls==1&&strstr(fb_status,"Close failed"));
}
static void recover_preview(void){assert(fb_retained_file&&sd.read_calls==1&&sd.close_calls==2);sd.fail_read=sd.fail_close=false;}
static void expect_preview_recovered(void){assert(fb_preview_count==FB_PREVIEW_BYTES&&sd.read_opens==2&&sd.read_calls==2);no_handles();}
static void check_preview_read_and_close_failure(void){
 static const controller_step steps[]={
  {FB_LIST,420,244,NULL},{FB_ACTIONS,100,332,fail_preview},
  {FB_PREVIEW,300,730,expect_preview_owned}, /* A second read is blocked. */
  {FB_PREVIEW,75,730,expect_preview_owned}, /* Back retries the retained close. */
  {FB_PREVIEW,75,730,recover_preview},{FB_ACTIONS,100,332,no_handles},
  {FB_PREVIEW,75,730,expect_preview_recovered},{FB_ACTIONS,75,730,NULL},
  {FB_LIST,75,730,NULL}
 };
 controller_disk_setup();RUN_CONTROLLER(steps);
}
static void fail_rename(void){assert(!strcmp(fb_editor,"read.txt!"));sd.fail_rename=true;}
static void retry_rename(void){assert(strstr(fb_status,"Rename failed")&&sd.rename_calls==1&&!sd.renames&&find(&sd,"/read.txt")>=0&&find(&sd,"/read.txt!")<0);sd.fail_rename=false;no_handles();}
static void expect_renamed(void){assert(sd.rename_calls==2&&sd.renames==1&&find(&sd,"/read.txt")<0&&find(&sd,"/read.txt!")>=0&&strstr(fb_status,"Renamed successfully"));no_handles();}
static void fail_move(void){assert(!strcmp(fb_path,"/Books"));sd.fail_rename=true;}
static void retry_move(void){assert(strstr(fb_status,"Move failed")&&sd.rename_calls==1&&!sd.renames&&find(&sd,"/read.txt")>=0&&find(&sd,"/Books/read.txt")<0);sd.fail_rename=false;no_handles();}
static void expect_moved(void){assert(sd.rename_calls==2&&sd.renames==1&&find(&sd,"/read.txt")<0&&find(&sd,"/Books/read.txt")>=0&&strstr(fb_status,"Moved successfully"));no_handles();}
static void check_rename_and_move_failures(void){
 static const controller_step rename_steps[]={
  {FB_LIST,420,244,NULL},{FB_ACTIONS,100,420,NULL},
  {FB_RENAME,140,250,NULL}, /* Append ! with the visible keyboard. */
  {FB_RENAME,350,730,fail_rename},{FB_RENAME,350,730,retry_rename},
  {FB_LIST,75,730,expect_renamed}
 };
 controller_disk_setup();RUN_CONTROLLER(rename_steps);
 static const controller_step move_steps[]={
  {FB_LIST,420,244,NULL},{FB_ACTIONS,100,508,NULL},
  {FB_DESTINATION,100,150,NULL},{FB_DESTINATION,300,730,fail_move},
  {FB_DESTINATION,300,730,retry_move},{FB_LIST,75,730,expect_moved}
 };
 controller_disk_setup();RUN_CONTROLLER(move_steps);
}
enum { COPY_READ_FAIL,COPY_WRITE_FAIL,COPY_SD_REMOVED,COPY_USB_REMOVED,
 COPY_SOURCE_OPEN_FAIL,COPY_TARGET_OPEN_FAIL,COPY_ABORT_RETRY,COPY_COMMIT_RETRY,COPY_SOURCE_CLOSE_RETRY };
static unsigned copy_failure;
static void inject_copy_failure(void){
 assert(fb_storage_index==1&&fb_volume==&usb.api.base&&fb_other_volume==&sd.api.base&&!strcmp(fb_path,"/Books"));
 switch(copy_failure){
  case COPY_READ_FAIL:sd.fail_read=true;break;
  case COPY_WRITE_FAIL:usb.fail_write=true;break;
  case COPY_SD_REMOVED:sd.drop_on_read=2;break;
  case COPY_USB_REMOVED:usb.drop_on_write=2;break;
  case COPY_SOURCE_OPEN_FAIL:sd.fail_open_read=true;break;
  case COPY_TARGET_OPEN_FAIL:usb.fail_open_write=true;break;
  case COPY_ABORT_RETRY:usb.fail_write=usb.fail_close=true;break;
  case COPY_COMMIT_RETRY:usb.fail_close=true;break;
  case COPY_SOURCE_CLOSE_RETRY:sd.fail_close=true;break;
  default:assert(false);
 }
}
static void expect_copy_failure(void){
 assert(find(&sd,"/read.txt")>=0&&sd.nodes[find(&sd,"/read.txt")].size==5000&&!sd.removals&&!sd.renames&&!usb.commits);
 assert(fb_storage_index==1&&fb_source_index==0&&!strcmp(fb_path,"/Books"));
 assert(find(&usb,"/Books/read.txt")<0);
 if(copy_failure==COPY_SOURCE_OPEN_FAIL){assert(!sd.read_opens&&!usb.write_opens&&!sd.close_calls&&!usb.close_calls&&strstr(fb_status,"Cannot open source"));}
 else if(copy_failure==COPY_TARGET_OPEN_FAIL){assert(sd.read_opens==1&&!usb.write_opens&&sd.close_calls==1&&!usb.close_calls&&strstr(fb_status,"Cannot create destination"));}
 else {assert(sd.read_opens==1&&usb.write_opens==1&&sd.close_calls==1&&usb.close_calls==1&&usb.aborts==1&&strstr(fb_status,"incomplete destination aborted"));}
 if(copy_failure==COPY_SD_REMOVED)assert(sd.offline&&sd.read_calls==2&&usb.write_calls>0);
 if(copy_failure==COPY_USB_REMOVED)assert(usb.offline&&usb.write_calls==2&&sd.read_calls==1);
 no_handles();sd.offline=usb.offline=false;
}
static void expect_copy_owned(void){
 disk *owner=copy_failure==COPY_SOURCE_CLOSE_RETRY?&sd:&usb;
 bool commit=copy_failure!=COPY_ABORT_RETRY;
 unsigned owned=0;
 assert(!portable_file_browser_safe()&&fb_grant.api==&usb.api&&fb_other_grant.api==&sd.api);
 assert(fb_storage_index==1&&find(&sd,"/read.txt")>=0&&!sd.removals);
 for(unsigned i=0;i<4;i++)if(fb_owned[i].handle){
  owned++;assert(fb_owned[i].api==&owner->api.base&&!fb_owned[i].directory&&fb_owned[i].commit==commit);
  assert(owner->handles[fb_owned[i].handle-1].used);
 }
 assert(owned==1&&sd.read_opens==1&&usb.write_opens==1);
 if(copy_failure==COPY_SOURCE_CLOSE_RETRY)assert(find(&usb,"/Books/read.txt")<0&&usb.aborts==1);
 else assert(find(&usb,"/Books/read.txt")>=0&&!usb.aborts&&!usb.commits);
}
static void allow_copy_close(void){expect_copy_owned();sd.fail_close=usb.fail_close=false;}
static void expect_copy_cancelled(void){
 assert(!fb_storage_index&&!strcmp(fb_path,"/")&&strstr(fb_status,"Copy cancelled"));
 assert(find(&sd,"/read.txt")>=0&&sd.read_opens==1&&usb.write_opens==1);no_handles();
 if(copy_failure==COPY_COMMIT_RETRY){
  int destination=find(&usb,"/Books/read.txt");assert(destination>=0&&usb.commits==1&&!usb.aborts&&usb.nodes[destination].size==5000);
  assert(!memcmp(sd.nodes[find(&sd,"/read.txt")].data,usb.nodes[destination].data,5000));
 }else assert(find(&usb,"/Books/read.txt")<0&&usb.aborts==1&&!usb.commits);
}
static void check_copy_failures(void){
 static const controller_step failed[]={
  {FB_LIST,420,244,NULL},{FB_ACTIONS,100,596,NULL},
  {FB_DESTINATION,420,50,NULL},{FB_DESTINATION,100,150,NULL},
  {FB_DESTINATION,300,730,inject_copy_failure},{FB_DESTINATION,75,730,expect_copy_failure},
  {FB_LIST,75,730,NULL}
 };
 for(copy_failure=COPY_READ_FAIL;copy_failure<=COPY_TARGET_OPEN_FAIL;copy_failure++){
  controller_disk_setup();RUN_CONTROLLER(failed);
 }
 static const controller_step retained[]={
  {FB_LIST,420,244,NULL},{FB_ACTIONS,100,596,NULL},
  {FB_DESTINATION,420,50,NULL},{FB_DESTINATION,100,150,NULL},
  {FB_DESTINATION,300,730,inject_copy_failure},
  {FB_DESTINATION,420,50,expect_copy_owned}, /* Switching cannot abandon the owner. */
  {FB_DESTINATION,300,730,expect_copy_owned}, /* Repeated HERE cannot reopen files. */
  {FB_DESTINATION,75,730,expect_copy_owned}, /* CANCEL retries a failed close. */
  {FB_DESTINATION,75,730,allow_copy_close},
  {FB_LIST,75,730,expect_copy_cancelled}
 };
 for(copy_failure=COPY_ABORT_RETRY;copy_failure<=COPY_SOURCE_CLOSE_RETRY;copy_failure++){
  controller_disk_setup();RUN_CONTROLLER(retained);
 }
}
enum { DELETE_REMOVE_FAIL,DELETE_LIST_FAIL,DELETE_CLOSE_FAIL };
static unsigned delete_failure;
static void inject_delete_failure(void){
 if(delete_failure==DELETE_REMOVE_FAIL)sd.fail_remove=true;
 else if(delete_failure==DELETE_LIST_FAIL)sd.fail_listing=true;
 else sd.fail_dir_close=true;
}
static void expect_delete_failure(void){
 assert(find(&sd,"/Tree")>=0&&find(&sd,"/Tree/Nested")>=0&&find(&sd,"/Tree/Nested/hidden")>=0&&find(&sd,"/read.txt")>=0&&!sd.removals);
 if(delete_failure==DELETE_CLOSE_FAIL){
  assert(!portable_file_browser_safe()&&sd.open_dirs==1&&!sd.remove_calls&&fb_grant.api==&sd.api&&strstr(fb_status,"Close failed"));
  unsigned owned=0;for(unsigned i=0;i<4;i++)if(fb_owned[i].handle){owned++;assert(fb_owned[i].api==&sd.api.base&&fb_owned[i].directory);}assert(owned==1);
 }else {no_handles();assert(delete_failure==DELETE_REMOVE_FAIL?sd.remove_calls==1:sd.remove_calls==0);}
}
static void recover_delete(void){expect_delete_failure();sd.fail_remove=sd.fail_listing=sd.fail_dir_close=false;}
static void expect_deleted(void){assert(find(&sd,"/Tree")<0&&find(&sd,"/Tree/Nested")<0&&find(&sd,"/Tree/Nested/hidden")<0&&find(&sd,"/read.txt")>=0&&sd.removals==3&&strstr(fb_status,"Deleted successfully"));no_handles();}
static void check_delete_failures(void){
 static const controller_step failed[]={
  {FB_LIST,420,244,NULL},{FB_ACTIONS,400,730,NULL},{FB_ACTIONS,100,150,NULL},
  {FB_DELETE,350,730,inject_delete_failure},{FB_NOTICE,75,730,recover_delete},
  {FB_LIST,420,244,no_handles},{FB_ACTIONS,400,730,NULL},{FB_ACTIONS,100,150,NULL},
  {FB_DELETE,350,730,NULL},{FB_LIST,75,730,expect_deleted}
 };
 static const controller_step retained[]={
  {FB_LIST,420,244,NULL},{FB_ACTIONS,400,730,NULL},{FB_ACTIONS,100,150,NULL},
  {FB_DELETE,350,730,inject_delete_failure},{FB_NOTICE,75,730,expect_delete_failure},
  {FB_NOTICE,75,730,recover_delete},{FB_LIST,420,244,no_handles},
  {FB_ACTIONS,400,730,NULL},{FB_ACTIONS,100,150,NULL},
  {FB_DELETE,350,730,NULL},{FB_LIST,75,730,expect_deleted}
 };
 for(delete_failure=DELETE_REMOVE_FAIL;delete_failure<=DELETE_CLOSE_FAIL;delete_failure++){
  controller_disk_setup();add(&sd,"/Tree",true,0);add(&sd,"/Tree/Nested",true,0);add(&sd,"/Tree/Nested/hidden",false,7);
  if(delete_failure==DELETE_CLOSE_FAIL)RUN_CONTROLLER(retained);else RUN_CONTROLLER(failed);
 }
}
static void check_controller_failures(void){
 check_absent_removed_and_listing();check_preview_read_and_close_failure();check_rename_and_move_failures();check_copy_failures();check_delete_failures();
 puts("Production app_main/raw touch: SD absence/removal/reinsert, listing/preview errors, rename/move/delete failures, copy read/write/removal/open failures and retained file/directory cleanup passed");
}

int main(void){
 init_disk(&sd);init_disk(&usb);add(&sd,"/read.txt",false,5000);add(&sd,"/existing.txt",false,3);add(&sd,"/empty",false,0);
 test_volume=sd.api.base;test_volume.struct_size=sizeof(test_volume);
 assert(app_module_init()==0&&fb_open());assert(portable_file_browser_close());fb_runtime=&runtime;assert(fb_load(0)&&fb_manage&&fb_paper);frame("writable-files");
 select_item("/read.txt");frame("actions");fb_choice=3;fbx_activate();assert(fb_mode==FB_RENAME);frame("rename");strcpy(fb_editor,"../bad");fb_filter_key(PWK_DONE);assert(!sd.renames&&fb_mode==FB_RENAME);strcpy(fb_editor,"existing.txt");fb_filter_key(PWK_DONE);assert(!sd.renames);strcpy(fb_editor,"new.txt");fb_filter_key(PWK_DONE);assert(sd.renames==1&&find(&sd,"/new.txt")>=0&&find(&sd,"/read.txt")<0);
 select_item("/new.txt");fb_choice=4;fbx_activate();assert(fb_mode==FB_DESTINATION);frame("move-destination");strcpy(fb_path,"/Books");fbx_commit_destination();assert(find(&sd,"/Books/new.txt")>=0&&find(&sd,"/new.txt")<0);no_handles();
 select_item("/Books");fbx_destination(true);strcpy(fb_path,"/Books");fbx_commit_destination();assert(fb_mode==FB_DESTINATION&&strstr(fb_status,"itself"));fbx_cancel();
 select_item("/Books/new.txt");fbx_destination(false);strcpy(fb_path,"/");fbx_commit_destination();assert(find(&sd,"/new.txt")>=0&&sd.commits==1);assert(!memcmp(sd.nodes[find(&sd,"/new.txt")].data,sd.nodes[find(&sd,"/Books/new.txt")].data,5000));
 select_item("/new.txt");fbx_destination(false);assert(fbx_swap()&&fb_storage_index==1);strcpy(fb_path,"/Books");fbx_commit_destination();assert(!fb_storage_index&&find(&usb,"/Books/new.txt")>=0&&usb.commits==1);no_handles();
 select_item("/new.txt");fbx_destination(false);assert(fbx_swap());strcpy(fb_path,"/Books");fbx_commit_destination();assert(strstr(fb_status,"exists")&&usb.commits==1);fbx_cancel();assert(!fb_storage_index);
 sd.fail_read=true;assert(!fbx_copy(&sd.api.base,"/new.txt",&usb.api.base,"/read-fail.txt")&&find(&usb,"/read-fail.txt")<0);sd.fail_read=false;
 usb.fail_write=true;assert(!fbx_copy(&sd.api.base,"/new.txt",&usb.api.base,"/write-fail.txt")&&find(&usb,"/write-fail.txt")<0);usb.fail_write=false;
 sd.grow=true;assert(!fbx_copy(&sd.api.base,"/new.txt",&usb.api.base,"/changed.txt")&&find(&usb,"/changed.txt")<0);sd.grow=false;
 usb.fail_close=true;assert(!fbx_copy(&sd.api.base,"/new.txt",&usb.api.base,"/pending.txt")&&!portable_file_browser_safe());assert(!portable_file_browser_close());usb.fail_close=false;assert(portable_file_browser_close()&&find(&usb,"/pending.txt")>=0);assert(fb_load(0));no_handles();
 sd.fail_close=true;assert(!fbx_copy(&sd.api.base,"/new.txt",&usb.api.base,"/source-close.txt")&&find(&usb,"/source-close.txt")<0&&!portable_file_browser_safe());assert(!portable_file_browser_close());sd.fail_close=false;assert(portable_file_browser_close()&&fb_load(0));no_handles();
 assert(fbx_copy(&sd.api.base,"/empty",&usb.api.base,"/empty")&&usb.nodes[find(&usb,"/empty")].size==0);no_handles();
 sd.fail_dir_close=true;assert(!fb_load(0)&&!portable_file_browser_safe());sd.fail_dir_close=false;assert(portable_file_browser_close()&&fb_load(0));no_handles();
 sd.fail_listing=true;assert(!fb_load(0));sd.fail_listing=false;assert(fb_load(0));
 select_item("/new.txt");fb_choice=6;fbx_activate();frame("delete-confirm");assert(fb_mode==FB_DELETE&&find(&sd,"/new.txt")>=0);fbp_touch(240,730);assert(fb_mode==FB_DELETE);fbp_touch(100,730);assert(fb_mode==FB_ACTIONS&&find(&sd,"/new.txt")>=0);
 fb_choice=6;fbx_activate();fbp_touch(350,730);assert(find(&sd,"/new.txt")<0&&fb_mode==FB_LIST);
 add(&sd,"/Tree",true,0);add(&sd,"/Tree/Nested",true,0);add(&sd,"/Tree/Nested/hidden",false,12);add(&sd,"/Tree/.dot",false,2);select_item("/Tree");fb_choice=6;fbx_activate();fb_choice=1;fbx_activate();assert(find(&sd,"/Tree")<0&&find(&sd,"/Tree/Nested/hidden")<0);no_handles();
 add(&sd,"/read.txt",false,5);select_item("/read.txt");handler_missing=true;fbx_open(false);assert(fb_mode==FB_NOTICE&&strstr(fb_status,"unavailable")&&!handler_calls);frame("handler-unavailable");handler_missing=false;
 fb_mode=FB_ACTIONS;handler_count=0;fbx_open(false);assert(fb_mode==FB_NOTICE&&strstr(fb_status,"No declared"));handler_count=8;fbx_open(true);assert(fb_mode==FB_HANDLERS);frame("handlers");fb_choice=5;fbx_move(1);assert(fb_handler_offset==6);fbx_move(-1);assert(!fb_handler_offset);handler_invalid=true;assert(!fbx_handler_page()&&fb_mode==FB_NOTICE);handler_invalid=false;
 handler_result_pending=true;handler_result_error=-7;fbx_resume_result();assert(!handler_result_pending&&strstr(fb_status,"-7"));handler_result_pending=true;handler_result_error=0;fbx_resume_result();assert(!strcmp(fb_status,"File handler returned"));
 handler_request_fail=true;fbx_open(false);fbx_dispatch(0);assert(fb_mode==FB_NOTICE&&!fb_terminal&&handler_calls==1);handler_request_fail=false;
 for(unsigned i=0;i<3;i++){
  assert(fbx_swap()&&fbx_swap());select_item("/read.txt");fbx_open(true);assert(fb_handler_grant.api&&fb_other_grant.api&&fb_grant.api);
  release_target=i==0?(const void*)&handlers:i==1?(const void*)&usb.api:(const void*)&sd.api;
  assert(!portable_file_browser_close());if(i==0)assert(fb_handler_grant.api);else if(i==1)assert(!fb_handler_grant.api&&fb_other_grant.api);else assert(!fb_handler_grant.api&&!fb_other_grant.api&&fb_grant.api);
  release_target=NULL;assert(portable_file_browser_close()&&fb_load(0));no_handles();
 }
 add(&sd,"/unadmitted.elf",false,4);select_item("/unadmitted.elf");fbx_open(false);assert(fb_mode==FB_NOTICE&&strstr(fb_status,"admit")&&!test_launches);
 select_item("/read.txt");handler_count=1;fbx_open(false);assert(fb_terminal&&handler_calls==2&&!strcmp(requested_path,"/sd/read.txt")&&!strcmp(requested_app,"reader0")&&!fb_grant.api&&!fb_handler_grant.api);no_handles();
 app_module_fini();assert(!test_grants&&!test_frames&&!test_subs);
 test_runtime_override=&runtime;test_polls=0;test_seen_modes=0;test_touch_script=deletion_cancel_script;assert(app_module_init()==0);app_main();app_module_fini();test_touch_script=NULL;test_runtime_override=NULL;
 assert((test_seen_modes&(1u<<FB_ACTIONS))&&(test_seen_modes&(1u<<FB_DELETE))&&find(&sd,"/read.txt")>=0&&test_launches==1);assert(!test_grants&&!test_frames&&!test_subs);
 check_controller_failures();
 puts("Paper file operations: rename/move/copy/delete, recursive folders, collision/cancel/close failures, explicit volume selection, handler paging/dispatch/unavailable and SD ELF admission passed");
}

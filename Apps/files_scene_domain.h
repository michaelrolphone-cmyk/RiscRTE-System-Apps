/* Extracted domain from file_browser_portable.inc; semantic UI/lifecycle supplied by files_scene.c. */
#ifdef PORTABLE_BLE_BROADCAST
#include "PortableBroadcastClient.h"
#endif
/* Portable NOVA-7 File Browser. Uses the existing volume ABI and shared model;
 * the provider owns its filesystem, authority and mutation semantics. */

#include "RiscRuntimeV1.h"
#include "PortableAppCustody.h"
#include "RiscStorageVolumeV1.h"
#include "RiscAppDataExportV1.h"
#include "FileBrowserModel.h"

#ifndef PORTABLE_TEXT_INPUT_CLIENT
#error Shared Files input requires the presenter handoff client
#endif
#include "PortableTextInputExtendedClient.h"
#ifdef PORTABLE_TEXT_INPUT_POLICY
#include "PortableTextInputPolicy.h"
#endif
#define FB_EDITOR_CHOICES 1u
#include "PortableFileBrowser.h"

#include "T5FileOpenApi.h"
#ifdef PORTABLE_FILE_BROWSER_SOURCE_HANDOFF
#include "RiscFileOpenSourceV1.h"
#include "RiscStorageVolumeStateV1.h"
#endif
#include <stdlib.h>
#ifdef PORTABLE_ALARM_CLIENT
#include "PortableAppSleep.h"
#endif
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define FB_PATH_CAP 512u
#ifndef PORTABLE_FILE_BROWSER_CAPABILITY
#define PORTABLE_FILE_BROWSER_CAPABILITY "storage.installed-files"
#endif
#ifndef PORTABLE_FILE_BROWSER_SECONDARY_CAPABILITY
#define PORTABLE_FILE_BROWSER_SECONDARY_CAPABILITY "storage.volume"
#endif
#define FB_MAX_PAGE_ROWS 16u
#define FB_PREVIEW_BYTES 192u
#ifndef PORTABLE_FILE_BROWSER_INSTANCE
#define PORTABLE_FILE_BROWSER_INSTANCE 0u
#endif
#ifndef FILE_BROWSER_RETURN_APP
#define FILE_BROWSER_RETURN_APP "springboard.elf"
#endif

enum { FB_LIST, FB_OPTIONS, FB_FILTER, FB_DETAIL, FB_PREVIEW, FB_ACTIONS, FB_RENAME, FB_DESTINATION, FB_DELETE, FB_HANDLERS, FB_NOTICE, FB_REPLACE, FB_SHARING };

static const risc_runtime_api_v1 *fb_runtime;
static const risc_storage_volume_api_v1 *fb_volume;
static risc_runtime_capability_v1 fb_grant;
static risc_storage_file_t fb_retained_file;
typedef struct {const risc_storage_volume_api_v1 *api;uint32_t handle;bool directory,commit;} fb_owned_handle;
static fb_owned_handle fb_owned[4];
#ifdef PORTABLE_FILE_BROWSER_TERTIARY_INSTANCE
#ifndef PORTABLE_FILE_BROWSER_SECONDARY_INSTANCE
#error A third root requires a configured second root
#endif
#define FB_LOCAL_ROOT_COUNT 3u
#elif defined(PORTABLE_FILE_BROWSER_SECONDARY_INSTANCE)
#define FB_LOCAL_ROOT_COUNT 2u
#else
#define FB_LOCAL_ROOT_COUNT 1u
#endif
#define FB_REMOTE_ROOT (FB_LOCAL_ROOT_COUNT<2u?2u:FB_LOCAL_ROOT_COUNT)
static risc_runtime_capability_v1 fb_root_grants[FB_LOCAL_ROOT_COUNT],fb_handler_grant;
static const risc_storage_volume_api_v1 *fb_root_volumes[FB_LOCAL_ROOT_COUNT];
static const t5_file_open_api_v1 *fb_handler_api;
static unsigned fb_storage_index,fb_source_index,fb_list_choice,fb_action_page,fb_handler_offset,fb_handler_count,fb_notice_return;
static bool fb_manage,fb_terminal,fb_publication_unknown;
#ifdef PORTABLE_FILE_BROWSER_READ_COPY_ONLY
#ifdef PORTABLE_FILE_BROWSER_HANDLERS
#error Read-copy profile cannot omit declared file handlers
#endif
static const bool fb_moving=false;
#else
static bool fb_moving;
#endif
static char fb_source_path[FB_PATH_CAP],fb_source_base[FB_PATH_CAP],fb_source_query[64];
#ifndef PORTABLE_FILE_BROWSER_READ_COPY_ONLY
static char fb_handler_path[FB_PATH_CAP];
#endif
static t5_file_handler_t fb_handlers[FB_MAX_PAGE_ROWS];
#ifdef PORTABLE_FILE_BROWSER_SOURCE_HANDOFF
static risc_runtime_capability_v1 fsource_grant;
static const risc_file_open_source_api_v1 *fsource_api;
enum {FSOURCE_NONE,FSOURCE_CHOOSE,FSOURCE_START,FSOURCE_LOCAL,FSOURCE_REMOTE,FSOURCE_SEALED,FSOURCE_CANCEL};
static unsigned fsource_state;
static bool fsource_remote,fsource_handoff;
static bool fsource_busy(void),fsource_close(void),fsource_back(void),fsource_handler_get(uint32_t,t5_file_handler_t*),fsource_resume_result(void);
static bool fsource_prefer(bool);
static void fsource_open(bool,bool),fsource_start(unsigned),fsource_tick(void),fsource_reset(void);
#ifdef PORTABLE_FILE_PEER
static void fsource_remote_done(bool);
#endif
#endif

static portable_text_client fb_text;
static risc_text_entry_state_v1_extended fb_text_state;
static unsigned fb_text_mode,fb_text_return_mode,fb_text_return_choice,fb_text_storage;
static bool fb_text_active,fb_text_closing,fb_text_home,fb_text_invalidated;
#ifdef PORTABLE_TEXT_INPUT_POLICY
static portable_text_checkpoint_client fb_text_checkpoint;
static bool fb_text_policy_continuity,fb_text_policy_boundary;
#endif
static bool fb_text_begin(unsigned,const char*),fb_text_close(void);
static void fb_text_step(void);
#ifdef PORTABLE_FILE_SHARING
#ifdef PORTABLE_FILE_PEER
static bool fpeer_mode,fbm_new_directory;
static bool fbm_mounted,fbm_applying,fbm_loading;
static bool fbm_before_load(int),fbm_swap(void);
static void fbm_unmount(void),fbm_leave(void),fbm_open(bool),fbm_rename(void),fbm_delete(void),fbm_new_folder(void);
static bool fbm_busy(void);
static void fbm_defer_dispatch(unsigned),fbm_replace_confirm(void),fbm_local_mkdir(void);
static bool fbm_preview(void),fbm_can_rename(void),fbm_can_delete(void),fbm_destination(bool),fbm_commit_destination(void),fbm_select(unsigned),fbm_back(void);
static char fbm_local_path[FB_PATH_CAP];
static void fpeer_open(void),fpeer_action(void);
#endif
static bool fsh_close(void),fsh_back(void);
static void fsh_tick(void),fsh_open(void),fsh_action(void);
#endif
static void fbx_rename(void);
static void fbx_cancel(void);
/* Only this tagged export contract permits changing a failed commit to memory
 * cancellation. Unknown status, CONTEXT (including readers), and RETAINED keep
 * the original close intent and owning grant. Never infer safety from text. */
static int32_t fbx_export_write_failure(const risc_storage_volume_api_v1 *api,uint32_t handle) {
 if(PORTABLE_APP_RETAINED())return 0;
 const risc_app_data_export_v1 *export_api=risc_app_data_export(api);
 if(!export_api || !export_api->write_status)return 0;
 int32_t status=export_api->write_status(api->context,handle);
 switch(status){
 case RISC_APP_DATA_NOT_FOUND:case RISC_APP_DATA_BUFFER_SMALL:case RISC_APP_DATA_INVALID:
 case RISC_APP_DATA_UNAVAILABLE:case RISC_APP_DATA_IO:case RISC_APP_DATA_NO_SPACE:
 case RISC_APP_DATA_STALE:return status;
 case RISC_APP_DATA_COMMIT_UNKNOWN:fb_publication_unknown=true;return status;
 default:return 0;
 }
}
static bool fbx_try_close_file(const risc_storage_volume_api_v1 *api,uint32_t handle,bool *commit) {
 if(PORTABLE_APP_RETAINED())return false;
 /* Browser-held export writers can become known failures before a Back retry.
  * Cancel those directly, without calling the publishing callback again. */
 if(*commit && fbx_export_write_failure(api,handle))*commit=false;
 if(PORTABLE_APP_RETAINED())return false;
 if(api->file_close(api->context,handle,*commit))return true;
 if(PORTABLE_APP_RETAINED())return false;
 if(*commit && fbx_export_write_failure(api,handle)){
  *commit=false;
  if(PORTABLE_APP_RETAINED())return false;
  return api->file_close(api->context,handle,false);
 }
 return false;
}
static bool fbx_close_owned(void) {
 if(PORTABLE_APP_RETAINED())return false;
 bool ok=true;
 for(unsigned i=0;i<4;i++)if(fb_owned[i].handle){
  fb_owned_handle *h=&fb_owned[i];const risc_storage_volume_api_v1_ext *ext=risc_storage_volume_extension(h->api);
  bool closed=h->directory?ext&&ext->dir_close_checked&&ext->dir_close_checked(h->api->context,h->handle):fbx_try_close_file(h->api,h->handle,&h->commit);
  if(closed)memset(h,0,sizeof(*h));else {PORTABLE_APP_RETAIN();if(PORTABLE_APP_RETAINED())return false;ok=false;}
 }
 return ok;
}
static bool fbx_hold(const risc_storage_volume_api_v1 *api,uint32_t handle,bool directory,bool commit) {
 for(unsigned i=0;i<4;i++)if(!fb_owned[i].handle){fb_owned[i]=(fb_owned_handle){api,handle,directory,commit};return false;}
 /* At most two copy files plus one directory can be live. */
 fb_runtime->diagnostic("FILE_BROWSER ownership capacity exceeded");for(;;)fb_runtime->yield_ms(50);
}
static bool fbx_close_file(const risc_storage_volume_api_v1 *api,uint32_t handle,bool commit) {
 if(PORTABLE_APP_RETAINED())return false;
 bool requested_commit=commit;
 if(!fbx_try_close_file(api,handle,&commit)){PORTABLE_APP_RETAIN();return fbx_hold(api,handle,false,commit);}
 /* Cleanup success is not publication success. */
 return commit==requested_commit;
}
static bool fbx_close_dir(const risc_storage_volume_api_v1 *api,uint32_t handle) {
 if(PORTABLE_APP_RETAINED())return false;
 const risc_storage_volume_api_v1_ext *ext=risc_storage_volume_extension(api);
 if(ext&&ext->dir_close_checked){
  if(ext->dir_close_checked(api->context,handle))return true;
  PORTABLE_APP_RETAIN();return fbx_hold(api,handle,true,false);
 }
 api->dir_close(api->context,handle);return true;
}
static risc_storage_dirent_v1 fb_rows[FB_MAX_PAGE_ROWS],fb_selected;

static unsigned fb_page_rows;
static unsigned fb_count,fb_choice,fb_page,fb_mode,fb_preview_count,fb_detail_offset;
static uint32_t fb_matched;
static bool fb_feedback;
static bool fb_dirty,fb_hidden,fb_more,fb_hex,fb_loaded;
static uint64_t fb_offset,fb_size;
static char fb_path[FB_PATH_CAP],fb_file_path[FB_PATH_CAP],fb_query[64],fb_editor[128],fb_status[96],fb_label[64];
static unsigned char fb_preview[FB_PREVIEW_BYTES];
static void fb_message(const char *message) {snprintf(fb_status,sizeof(fb_status),"%s",message);fb_dirty=true;fb_feedback=true;}
/* Local browser callbacks can run while a private peer owns the radio.
 * Scope only synchronous provider work; never hold this lease across peer
 * ticks, network cancellation, POLL, or application handoff. */
static bool fb_io_pending;
static bool fb_service_begin(void) {
 fb_io_pending=false;
 if(!files_scene_alive()||!files_scene_background_pause()||!files_scene_alive())return false;
#ifdef PORTABLE_FILE_SHARING
 if(!portable_file_browser_services_begin()){
  fb_io_pending=!fb_terminal&&!PORTABLE_APP_RETAINED();
  if(fb_io_pending)fb_message("Storage service busy. Retry.");
  return false;
 }
#endif
 return true;
}
static bool fb_service_end(void) {
#ifdef PORTABLE_FILE_SHARING
 return portable_file_browser_services_end();
#else
 return true;
#endif
}
static bool fb_service_local(void) {
#ifdef PORTABLE_FILE_PEER
 return fb_storage_index!=FB_REMOTE_ROOT;
#else
 return true;
#endif
}
static bool fb_api_valid(const risc_storage_volume_api_v1 *p) {
 if(!p || p->api_version!=1 || p->struct_size<sizeof(*p) || !p->refresh || !p->ready || !p->label || !p->stat || !p->dir_open || !p->dir_next || !p->dir_close || !p->file_open_read || !p->file_read || !p->file_close || !p->last_error)return false;
#ifdef PORTABLE_FILE_BROWSER_READ_COPY_ONLY
 const risc_storage_volume_api_v1_ext *ext=risc_storage_volume_extension(p);
 if(p->remove||(ext&&(ext->rename||ext->mkdir)))return false;
#endif
 return true;
}
/* Probe only configured, granted generic roots. A present capability remains
 * selectable when its media is absent; ready/refresh report that state. */
static const char *fb_root_capability(unsigned index){
 (void)index;
#ifdef PORTABLE_FILE_BROWSER_TERTIARY_INSTANCE
 if(index==2)return PORTABLE_FILE_BROWSER_TERTIARY_CAPABILITY;
#endif
#ifdef PORTABLE_FILE_BROWSER_SECONDARY_INSTANCE
 if(index==1)return PORTABLE_FILE_BROWSER_SECONDARY_CAPABILITY;
#endif
 return PORTABLE_FILE_BROWSER_CAPABILITY;
}
static uint64_t fb_root_instance(unsigned index){
 (void)index;
#ifdef PORTABLE_FILE_BROWSER_TERTIARY_INSTANCE
 if(index==2)return PORTABLE_FILE_BROWSER_TERTIARY_INSTANCE;
#endif
#ifdef PORTABLE_FILE_BROWSER_SECONDARY_INSTANCE
 if(index==1)return PORTABLE_FILE_BROWSER_SECONDARY_INSTANCE;
#endif
 return PORTABLE_FILE_BROWSER_INSTANCE;
}
static const risc_storage_volume_api_v1 *fb_local_volume(unsigned index){
 if(index>=FB_LOCAL_ROOT_COUNT)return NULL;
 return index==fb_storage_index?fb_volume:fb_root_volumes[index];
}
/* Clean optional absence is distinguishable from malformed/dirty results. */
static int fb_acquire_root(unsigned index,risc_runtime_capability_v1 *grant){
 if(!files_scene_alive())return -1;
 if(grant->api||grant->slot||grant->generation){PORTABLE_APP_RETAIN();return -1;}
 *grant=(risc_runtime_capability_v1){.struct_size=sizeof(*grant)};
 bool ok=fb_runtime->acquire(fb_root_capability(index),1,fb_root_instance(index),grant);
 if(!files_scene_alive())return -1;
 if(grant->struct_size!=sizeof(*grant)||
    (ok?(!grant->api||!grant->slot||!grant->generation):
        (grant->api||grant->slot||grant->generation))){PORTABLE_APP_RETAIN();return -1;}
 if(!ok)return 0;
 if(!fb_api_valid(grant->api)){PORTABLE_APP_RETAIN();return -1;}
 return 1;
}
static bool fb_acquire(void) {
 if(!files_scene_alive())return false;
 if(fb_volume)return true;
 if(fb_grant.api){PORTABLE_APP_RETAIN();return false;}
 if(fb_acquire_root(fb_storage_index,&fb_grant)!=1)return false;
 fb_volume=fb_grant.api;return true;
}
bool portable_file_browser_safe(void) {
#ifdef PORTABLE_TEXT_INPUT_POLICY
 if((fb_text_active&&!fb_text_policy_boundary)||fb_text.retained||portable_text_adapter_retained())return false;
#else
 if(fb_text_active||fb_text.retained||portable_text_adapter_retained())return false;
#endif
 if(PORTABLE_APP_RETAINED())return false;
 if(fb_retained_file)return false;
 for(unsigned i=0;i<4;i++)if(fb_owned[i].handle)return false;
 return true;
}
bool portable_file_browser_close(void) {
 if(PORTABLE_APP_RETAINED())return false;
#ifdef PORTABLE_TEXT_INPUT_POLICY
 /* Only this synchronous, settled policy dispatch may release storage/radio
  * resources while keeping the fully paused host and its draft alive. */
 if(!(fb_text_policy_boundary&&fb_text_checkpoint.paused))
#endif
 if(!fb_text_close())return false;
#ifdef PORTABLE_FILE_SHARING
 if(!fsh_close())return false;
#endif
#ifdef PORTABLE_FILE_BROWSER_SOURCE_HANDOFF
 if(!fsource_close())return false;
#endif
 if(!fbx_close_owned())return false;
 if(fb_retained_file) {
  if(!fb_volume || !fb_volume->file_close(fb_volume->context,fb_retained_file,true)){PORTABLE_APP_RETAIN();return false;}
  fb_retained_file=0;
 }
 if(fb_handler_grant.api && !fb_runtime->release(&fb_handler_grant))return false;
 memset(&fb_handler_grant,0,sizeof(fb_handler_grant));fb_handler_api=NULL;
 for(unsigned i=0;i<FB_LOCAL_ROOT_COUNT;i++){
  if(fb_root_grants[i].api&&!fb_runtime->release(&fb_root_grants[i]))return false;
  memset(&fb_root_grants[i],0,sizeof(fb_root_grants[i]));fb_root_volumes[i]=NULL;
 }
 if(fb_grant.api && !fb_runtime->release(&fb_grant))return false;
 memset(&fb_grant,0,sizeof(fb_grant));fb_volume=NULL;
 return true;
}
static void fb_error(const char *fallback);
static void fb_error_scoped(const char *fallback) {
 if(PORTABLE_APP_RETAINED())return;
 char message[sizeof(fb_status)]={0};
 if(fb_volume && fb_volume->last_error(fb_volume->context,message,sizeof(message)) && message[0] && memchr(message,0,sizeof(message)))fb_message(message);
 else fb_message(fallback);
}
static void fb_error(const char *fallback) {
 bool scope=fb_service_local();if(scope&&!fb_service_begin())return;
 fb_error_scoped(fallback);
 if(scope)(void)fb_service_end();
}
static bool fb_ready(void);
static bool fb_ready_scoped(void) {
 if(!portable_file_browser_safe()){
  if(fb_publication_unknown)fb_message("Copy publication unknown; cleanup retained. Back retries cleanup.");
  else fb_error("Close failed. Back retries safely.");
  return false;
 }
 if(!fb_acquire()){fb_message("Storage unavailable. Try Refresh.");return false;}
 if(!fb_volume->refresh(fb_volume->context) || !fb_volume->ready(fb_volume->context)){fb_error("Storage unavailable. Try Refresh.");return false;}
 memset(fb_label,0,sizeof(fb_label));
 if(!fb_volume->label(fb_volume->context,fb_label,sizeof(fb_label)) || !memchr(fb_label,0,sizeof(fb_label)))strcpy(fb_label,"Storage");
 return true;
}
static bool fb_ready(void) {
 bool scope=fb_service_local();if(scope&&!fb_service_begin())return false;
 bool result=fb_ready_scoped();
 return (!scope||fb_service_end())&&result;
}
static int fb_compare(const risc_storage_dirent_v1 *a,const risc_storage_dirent_v1 *b) {
 int order=fb_name_compare(a->name,a->is_directory!=0,b->name,b->is_directory!=0);
 /* Natural ties (A/a, 1/01) need a stable total order for page cursors. */
 return order?order:strcmp(a->name,b->name);
}
/* Streaming bounded-page selection uses constant RAM, without truncating the
 * folder inventory. Backward pages use the same ordering in reverse. */
static bool fb_load(int direction);
static bool fb_load_scoped(int direction) {
#ifdef PORTABLE_FILE_PEER
 if(fbm_mounted&&fb_storage_index==FB_REMOTE_ROOT&&!fbm_before_load(direction))return false;
#endif
 if(risc_protected_settings_path(fb_path)){fb_count=0;fb_loaded=false;fb_message("Protected system settings");return false;}
 risc_storage_dirent_v1 cursor={0};bool bounded=direction && fb_count;
 if(bounded)cursor=fb_rows[direction>0?fb_count-1:0];
 fb_count=0;fb_matched=0;fb_more=false;fb_loaded=false;
 if(!fb_ready())return false;
 risc_storage_dir_t directory=fb_volume->dir_open(fb_volume->context,fb_path);
 if(!directory){fb_error("Cannot open folder. Back or Refresh.");return false;}
 risc_storage_dirent_v1 item;unsigned examined=0;bool invalid=false;
 while(fb_volume->dir_next(fb_volume->context,directory,&item)) {
  if((++examined&63u)==0)fb_runtime->yield_ms(1);
  if(!fb_name_valid(item.name,sizeof(item.name)) || item.is_directory>1){invalid=true;break;}
  if(risc_protected_settings_path(item.name))continue;
  if(fb_mode==FB_DESTINATION && !item.is_directory)continue;
  if((!fb_hidden && item.name[0]=='.') || !fb_contains(item.name,fb_query))continue;
  if(fb_matched<UINT32_MAX)++fb_matched;
  int order=bounded?fb_compare(&item,&cursor):1;
  if(bounded && (direction>0?order<=0:order>=0))continue;
  unsigned pos=0;
  while(pos<fb_count && (direction<0?fb_compare(&fb_rows[pos],&item)>0:fb_compare(&fb_rows[pos],&item)<0))++pos;
  if(pos<fb_page_rows) {
   if(fb_count<fb_page_rows)++fb_count;else fb_more=true;
   for(unsigned j=fb_count-1;j>pos;j--)fb_rows[j]=fb_rows[j-1];
   fb_rows[pos]=item;
  } else fb_more=true;
 }
 const risc_storage_volume_api_v1_ext *ext=risc_storage_volume_extension(fb_volume);
 char error[96]={0};bool failed=ext&&ext->handle_error?ext->handle_error(fb_volume->context,directory,true)!=0:fb_volume->last_error(fb_volume->context,error,sizeof(error)) && error[0];
 if(!fbx_close_dir(fb_volume,directory))failed=true;
 if(invalid || failed){fb_count=0;fb_message(invalid?"Invalid directory data. Try Refresh.":"Folder read failed. Try Refresh.");return false;}
 if(direction<0)for(unsigned i=0;i<fb_count/2;i++){item=fb_rows[i];fb_rows[i]=fb_rows[fb_count-1-i];fb_rows[fb_count-1-i]=item;}
 if(direction<0){if(fb_page)fb_page--;fb_more=true;}
 else if(direction>0 && fb_count)fb_page++;
 else if(!direction)fb_page=0;
 fb_choice=0;fb_loaded=true;
 fb_manage=
#ifdef PORTABLE_FILE_BROWSER_SOURCE_HANDOFF
 true ||
#endif
#ifdef PORTABLE_FILE_PEER
 fb_storage_index==FB_REMOTE_ROOT ||
#endif
 fb_volume->remove || fb_volume->file_open_write || (risc_storage_volume_extension(fb_volume)&&risc_storage_volume_extension(fb_volume)->rename);
 fb_message(fb_mode==FB_DESTINATION?"Choose folder; HERE confirms":!fb_count?(fb_query[0]?"No matching files":"This folder is empty"):fb_manage?"Select item or open options":"Browse files / read-only volume");fb_feedback=false;
 return true;
}
static bool fb_load(int direction) {
 bool scope=fb_service_local();if(scope&&!fb_service_begin())return false;
 bool result=fb_load_scoped(direction);
 return (!scope||fb_service_end())&&result;
}
static void fb_page_next(void){if(!fb_loaded){(void)fb_load(0);return;}if(fb_more)(void)fb_load(1);else fb_message("Last page");}
static void fb_page_previous(void){if(!fb_loaded){(void)fb_load(0);return;}if(fb_page)(void)fb_load(-1);else fb_message("First page");}
static void fb_size_text(uint64_t value,char *out,size_t cap) {
 /* Avoid target's unsupported 64-bit printf while preserving exact size. */
 char reversed[21];unsigned n=0;
 do {
  uint64_t quotient=0;unsigned remainder=0;
  for(unsigned bit=64;bit;bit--) {
   remainder=remainder*2u+(unsigned)((value>>(bit-1))&1u);
   if(remainder>=10){remainder-=10;quotient|=UINT64_C(1)<<(bit-1);}
  }
  reversed[n++]=(char)('0'+remainder);value=quotient;
 }while(value && n<sizeof(reversed));
 if(n+7>=cap){if(cap)out[0]=0;return;}for(unsigned i=0;i<n;i++)out[i]=reversed[n-1-i];memcpy(out+n," bytes",7);
}
static bool fb_preview_read(void);
static bool fb_preview_read_scoped(void) {
#ifdef PORTABLE_FILE_PEER
 if(fb_storage_index==FB_REMOTE_ROOT)return fbm_preview();
#endif
 if(risc_protected_settings_path(fb_file_path)){fb_preview_count=0;fb_message("Protected system settings");return false;}
 fb_preview_count=0;
 if(!fb_ready())return false;
 uint64_t size=0;bool directory=false;
 if(!fb_volume->stat(fb_volume->context,fb_file_path,&size,&directory) || directory){fb_error("File unavailable. Back to refresh.");return false;}
 fb_size=size;if(fb_offset>=size && fb_offset)fb_offset=0;
 risc_storage_file_t file=fb_volume->file_open_read(fb_volume->context,fb_file_path,&size);
 if(!file){fb_error("Cannot read file. Try again.");return false;}
 bool ok=size==fb_size;uint64_t skipped=0;
 while(ok && skipped<fb_offset) {
  size_t wanted=(size_t)((fb_offset-skipped)<sizeof(fb_preview)?fb_offset-skipped:sizeof(fb_preview));
  size_t got=fb_volume->file_read(fb_volume->context,file,fb_preview,wanted);
  if(!got || got>wanted){ok=false;break;}skipped+=got;fb_runtime->yield_ms(1);
 }
 unsigned wanted=fb_hex?64u:FB_PREVIEW_BYTES;
 if(fb_size-fb_offset<wanted)wanted=(unsigned)(fb_size-fb_offset);
 while(ok && fb_preview_count<wanted) {
  unsigned remaining=wanted-fb_preview_count;
  size_t got=fb_volume->file_read(fb_volume->context,file,fb_preview+fb_preview_count,remaining);
  if(!got || got>remaining){ok=false;break;}fb_preview_count+=(unsigned)got;
 }
 if(!fb_volume->file_close(fb_volume->context,file,true)){fb_retained_file=file;fb_preview_count=0;fb_error("Close failed. Back retries safely.");return false;}
 if(!ok){fb_preview_count=0;fb_message("File changed or read failed. Retry.");return false;}
 fb_message(size?"Preview only / no execution":"Empty file");return true;
}
static bool fb_preview_read(void) {
 bool scope=fb_service_local();if(scope&&!fb_service_begin())return false;
 bool result=fb_preview_read_scoped();
 return (!scope||fb_service_end())&&result;
}
static void fb_activate_file(unsigned choice) {
#ifdef PORTABLE_FILE_PEER
 if(fbm_mounted&&fbm_busy())return;
#endif
 if(!fb_loaded || choice>=fb_count)return;
 fb_selected=fb_rows[choice];
 if(!fb_join(fb_path,fb_selected.name,fb_file_path,sizeof(fb_file_path))){fb_message("Path too long. Item not opened.");return;}
 if(fb_selected.is_directory){strcpy(fb_path,fb_file_path);fb_query[0]=0;(void)fb_load(0);return;}
 fb_mode=fb_manage?FB_ACTIONS:FB_DETAIL;fb_choice=0;fb_action_page=0;fb_detail_offset=0;fb_size=fb_selected.size;fb_offset=0;fb_hex=false;fb_message("Preview files without executing them");
}
static void fb_filter_apply(void) {
 if(fb_mode==FB_RENAME){fbx_rename();return;}
 if(strlen(fb_editor)>=sizeof(fb_query)){fb_message("Filter is full");return;}
 strcpy(fb_query,fb_editor);fb_mode=FB_LIST;(void)fb_load(0);
}
static bool fb_back(void) {
 if(fb_text_active){fb_text_state.state=RISC_TEXT_ENTRY_CANCELLED;fb_text_closing=true;fb_text_step();return false;}
#ifdef PORTABLE_FILE_BROWSER_SOURCE_HANDOFF
 if(fsource_back())return false;
#endif
#ifdef PORTABLE_FILE_PEER
 fbm_new_directory=false;
 if(fbm_mounted&&fbm_back())return false;
#endif
#ifdef PORTABLE_FILE_SHARING
 if(fb_mode==FB_SHARING)return fsh_back();
#endif
 if(!portable_file_browser_safe()){
  if(!portable_file_browser_close()){
   if(fb_publication_unknown)fb_message("Copy publication unknown; cleanup retained. Back retries cleanup.");
   else fb_error("Close failed. Back retries safely.");
   return false;
  }
  fb_message(fb_publication_unknown?"Copy publication unknown. Reload destination.":"Storage closed. Refresh to retry.");
 }
 if(fb_mode==FB_REPLACE){fb_mode=FB_DESTINATION;fb_choice=0;fb_dirty=true;return false;}
 if(fb_mode==FB_NOTICE){fb_mode=fb_notice_return;fb_dirty=true;return false;}
 if(fb_mode==FB_DESTINATION){fbx_cancel();return false;}
 if(fb_mode==FB_RENAME||fb_mode==FB_DELETE||fb_mode==FB_HANDLERS){fb_mode=FB_ACTIONS;fb_choice=0;fb_dirty=true;return false;}
 if(fb_mode==FB_PREVIEW){fb_mode=fb_manage?FB_ACTIONS:FB_DETAIL;fb_dirty=true;return false;}
 if(fb_mode!=FB_LIST){fb_mode=FB_LIST;fb_choice=0;fb_dirty=true;return false;}
#ifdef PORTABLE_FILE_PEER
 if(fbm_mounted&&fb_storage_index==FB_REMOTE_ROOT&&!strcmp(fb_path,"/")&&!fbm_loading){fbm_leave();strcpy(fb_path,fbm_local_path);fb_query[0]=0;(void)fb_load(0);return false;}
#endif
 if(fb_parent(fb_path)){fb_query[0]=0;(void)fb_load(0);return false;}
 if(!portable_file_browser_close()){fb_message("Storage release failed. Retry Back.");return false;}
 files_scene_exit_requested=true;
 return true;
}
static const char *const fb_options[]={"Find by name","Refresh folder","Show hidden","Go to root","Selected item","Switch volume"
#ifdef PORTABLE_FILE_SHARING
 ,"Share over Wi-Fi"
#ifdef PORTABLE_FILE_PEER
 ,"Nearby files","New folder","Disconnect nearby"
#endif
#endif
};
static unsigned fb_option_count(void){
#ifdef PORTABLE_FILE_PEER
 return fbm_mounted?10:9;
#endif
#ifdef PORTABLE_FILE_SHARING
 return 7;
#endif
#ifdef PORTABLE_FILE_BROWSER_SECONDARY_INSTANCE
 return 6; /* A read-only primary still needs access to the second volume. */
#else
 return fb_manage?6:4;
#endif
}
static void fbx_actions(void);
static void fbx_switch(void);
static bool fbx_swap(void);
static void fb_option(unsigned choice) {
#ifdef PORTABLE_FILE_PEER
 if(choice!=9&&fbm_mounted&&fbm_busy())return;
#endif
#ifdef PORTABLE_FILE_PEER
 if(choice==9){fb_mode=FB_LIST;(void)fsh_back();return;}
 if(choice==8){fbm_new_folder();return;}
 if(choice==7){if(fbm_mounted){if(fb_storage_index!=FB_REMOTE_ROOT){while(fb_storage_index!=FB_REMOTE_ROOT)if(!fbx_swap())return;}strcpy(fb_path,"/");fb_mode=FB_LIST;(void)fb_load(0);}else fpeer_open();return;}
#endif
#ifdef PORTABLE_FILE_SHARING
 if(choice==6){
#ifdef PORTABLE_FILE_PEER
 if(fbm_mounted){fb_message("Disconnect nearby before starting a share");return;}
#endif
 fsh_open();return;}
#endif
 if(choice==4){fb_choice=fb_list_choice;fbx_actions();return;}
 if(choice==5){fbx_switch();return;}
 if(choice==0){strcpy(fb_editor,fb_query);
 (void)fb_text_begin(FB_FILTER,"Find file");
 }
 else {if(choice==2)fb_hidden=!fb_hidden;if(choice==3){strcpy(fb_path,"/");fb_query[0]=0;}fb_mode=FB_LIST;(void)fb_load(0);}
}
static void fb_preview_page(int direction) {
#ifdef PORTABLE_FILE_PEER
 if(fbm_mounted&&fbm_busy())return;
#endif
 unsigned step=fb_hex?64u:FB_PREVIEW_BYTES;
 if(direction>0){if(fb_offset+fb_preview_count>=fb_size || !fb_preview_count){fb_message("End of file");return;}fb_offset+=step;}
 else {if(!fb_offset){fb_message("Start of file");return;}fb_offset=fb_offset>step?fb_offset-step:0;}
 (void)fb_preview_read();
}
static void fb_activate(void);
#include "files_scene_operations.inc"


#ifdef PORTABLE_FILE_SHARING
#include "files_scene_sharing.inc"
#endif
#ifdef PORTABLE_FILE_BROWSER_SOURCE_HANDOFF
#include "files_scene_source.inc"
#endif
#include "files_scene_shared_input.inc"

static void fb_activate(void) {
#ifdef PORTABLE_FILE_BROWSER_SOURCE_HANDOFF
 if(fsource_busy())return;
#endif
#ifdef PORTABLE_FILE_PEER
 if(fbm_mounted&&fbm_busy())return;
#endif
#ifdef PORTABLE_FILE_SHARING
 if(fb_mode==FB_SHARING){
#ifdef PORTABLE_FILE_PEER
 if(fpeer_mode){fpeer_action();return;}
#endif
 if(fb_choice)(void)fsh_back();else fsh_action();return;}
#endif
 if(fb_mode>=FB_ACTIONS){fbx_activate();fb_dirty=true;return;}
 if(fb_mode==FB_LIST)fb_activate_file(fb_choice);
 else if(fb_mode==FB_OPTIONS)fb_option(fb_choice);
 else if(fb_mode==FB_FILTER){
 (void)fb_text_begin(FB_FILTER,"Find file");
 }
 else if(fb_mode==FB_DETAIL){fb_mode=FB_PREVIEW;fb_offset=0;(void)fb_preview_read();}
 else {fb_hex=!fb_hex;fb_offset=0;(void)fb_preview_read();}
 fb_dirty=true;
}


#ifdef PORTABLE_FILE_SHARING

#endif

/* Portable NOVA-7 File Browser. Uses the existing volume ABI and shared model;
 * the provider owns its filesystem, authority and mutation semantics. */
#include "T5AppApi.h"
#include "RiscRuntimeV1.h"
#include "RiscStorageVolumeV1.h"
#include "FileBrowserModel.h"
#include "PortableNovaUi.h"
#include "PortableWatchKeyboard.h"
#include "PortableFileBrowser.h"
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
#define FB_PAGE_ROWS 2u
#define FB_PREVIEW_BYTES 192u
#ifndef PORTABLE_FILE_BROWSER_INSTANCE
#define PORTABLE_FILE_BROWSER_INSTANCE 0u
#endif
#ifndef FILE_BROWSER_RETURN_APP
#define FILE_BROWSER_RETURN_APP "springboard.elf"
#endif

enum { FB_LIST, FB_OPTIONS, FB_FILTER, FB_DETAIL, FB_PREVIEW };
static const t5_app_api_v1 *fb_app;
static const risc_runtime_api_v1 *fb_runtime;
static const risc_storage_volume_api_v1 *fb_volume;
static risc_runtime_capability_v1 fb_grant;
static risc_storage_file_t fb_retained_file;
static risc_storage_dirent_v1 fb_rows[FB_PAGE_ROWS],fb_selected;
static unsigned fb_count,fb_choice,fb_page,fb_mode,fb_key_page,fb_preview_count,fb_detail_offset;
static uint32_t fb_matched;
static bool fb_dirty,fb_armed,fb_hidden,fb_more,fb_hex,fb_loaded,fb_contact,fb_drag;
static int fb_contact_y;
static uint64_t fb_offset,fb_size;
static char fb_path[FB_PATH_CAP],fb_file_path[FB_PATH_CAP],fb_query[64],fb_editor[64],fb_status[96],fb_label[64];
static unsigned char fb_preview[FB_PREVIEW_BYTES];
static void fb_message(const char *message) {snprintf(fb_status,sizeof(fb_status),"%s",message);fb_dirty=true;}
static bool fb_api_valid(const risc_storage_volume_api_v1 *p) {
 return p && p->api_version==1 && p->struct_size>=sizeof(*p) && p->refresh && p->ready && p->label && p->stat && p->dir_open && p->dir_next && p->dir_close && p->file_open_read && p->file_read && p->file_close && p->last_error;
}
static bool fb_acquire(void) {
 if(fb_volume)return true;
 if(fb_grant.api){if(!fb_runtime->release(&fb_grant))return false;memset(&fb_grant,0,sizeof(fb_grant));}
 fb_grant=(risc_runtime_capability_v1){.struct_size=sizeof(fb_grant)};
 if(!fb_runtime->acquire(PORTABLE_FILE_BROWSER_CAPABILITY,1,PORTABLE_FILE_BROWSER_INSTANCE,&fb_grant))return false;
 if(!fb_api_valid(fb_grant.api))return false;
 fb_volume=fb_grant.api;return true;
}
bool portable_file_browser_safe(void) {return !fb_retained_file;}
bool portable_file_browser_close(void) {
 if(fb_retained_file) {
  if(!fb_volume || !fb_volume->file_close(fb_volume->context,fb_retained_file,true))return false;
  fb_retained_file=0;
 }
 if(fb_grant.api && !fb_runtime->release(&fb_grant))return false;
 memset(&fb_grant,0,sizeof(fb_grant));fb_volume=NULL;return true;
}
static void fb_error(const char *fallback) {
 char message[sizeof(fb_status)]={0};
 if(fb_volume && fb_volume->last_error(fb_volume->context,message,sizeof(message)) && message[0] && memchr(message,0,sizeof(message)))fb_message(message);
 else fb_message(fallback);
}
static bool fb_ready(void) {
 if(fb_retained_file){fb_error("Close failed. Back retries safely.");return false;}
 if(!fb_acquire()){fb_message("Storage unavailable. Try Refresh.");return false;}
 if(!fb_volume->refresh(fb_volume->context) || !fb_volume->ready(fb_volume->context)){fb_error("Storage unavailable. Try Refresh.");return false;}
 memset(fb_label,0,sizeof(fb_label));
 if(!fb_volume->label(fb_volume->context,fb_label,sizeof(fb_label)) || !memchr(fb_label,0,sizeof(fb_label)))strcpy(fb_label,"Storage");
 return true;
}
static int fb_compare(const risc_storage_dirent_v1 *a,const risc_storage_dirent_v1 *b) {
 int order=fb_name_compare(a->name,a->is_directory!=0,b->name,b->is_directory!=0);
 /* Natural ties (A/a, 1/01) need a stable total order for page cursors. */
 return order?order:strcmp(a->name,b->name);
}
/* Streaming top-two page selection uses constant RAM, without truncating the
 * folder inventory. Backward pages use the same ordering in reverse. */
static bool fb_load(int direction) {
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
  if((!fb_hidden && item.name[0]=='.') || !fb_contains(item.name,fb_query))continue;
  if(fb_matched<UINT32_MAX)++fb_matched;
  int order=bounded?fb_compare(&item,&cursor):1;
  if(bounded && (direction>0?order<=0:order>=0))continue;
  unsigned pos=0;
  while(pos<fb_count && (direction<0?fb_compare(&fb_rows[pos],&item)>0:fb_compare(&fb_rows[pos],&item)<0))++pos;
  if(pos<FB_PAGE_ROWS) {
   if(fb_count<FB_PAGE_ROWS)++fb_count;else fb_more=true;
   for(unsigned j=fb_count-1;j>pos;j--)fb_rows[j]=fb_rows[j-1];
   fb_rows[pos]=item;
  } else fb_more=true;
 }
 char error[96]={0};bool failed=fb_volume->last_error(fb_volume->context,error,sizeof(error)) && error[0];
 fb_volume->dir_close(fb_volume->context,directory);
 if(invalid || failed){fb_count=0;fb_message(invalid?"Invalid directory data. Try Refresh.":"Folder read failed. Try Refresh.");return false;}
 if(direction<0 && fb_count==2){item=fb_rows[0];fb_rows[0]=fb_rows[1];fb_rows[1]=item;}
 if(direction<0){if(fb_page)fb_page--;fb_more=true;}
 else if(direction>0 && fb_count)fb_page++;
 else if(!direction)fb_page=0;
 fb_choice=0;fb_loaded=true;
 fb_message(!fb_count?(fb_query[0]?"No matching files":"This folder is empty"):"Browse files / read-only volume");
 return true;
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
static bool fb_preview_read(void) {
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
static void fb_activate_file(unsigned choice) {
 if(!fb_loaded || choice>=fb_count)return;
 fb_selected=fb_rows[choice];
 if(!fb_join(fb_path,fb_selected.name,fb_file_path,sizeof(fb_file_path))){fb_message("Path too long. Item not opened.");return;}
 if(fb_selected.is_directory){strcpy(fb_path,fb_file_path);fb_query[0]=0;(void)fb_load(0);return;}
 fb_mode=FB_DETAIL;fb_detail_offset=0;fb_size=fb_selected.size;fb_offset=0;fb_hex=false;fb_message("Preview files without executing them");
}
static void fb_filter_key(unsigned key) {
 size_t n=strlen(fb_editor);
 if(key<PWK_CHARACTERS){unsigned ch=portable_watch_key_character(fb_key_page,key);if(ch && n+1<sizeof(fb_editor)){fb_editor[n]=(char)ch;fb_editor[n+1]=0;}else if(ch)fb_message("Filter is full");}
 else if(key==PWK_PAGE)fb_key_page=(fb_key_page+1)%PWK_PAGES;
 else if(key==PWK_DELETE){if(n)fb_editor[n-1]=0;}
 else if(key==PWK_DONE){strcpy(fb_query,fb_editor);fb_mode=FB_LIST;(void)fb_load(0);}
 fb_dirty=true;
}
static bool fb_back(void) {
 if(fb_retained_file){if(!portable_file_browser_close()){fb_error("Close failed. Back retries safely.");return false;}fb_message("Storage closed. Refresh to retry.");}
 if(fb_mode==FB_PREVIEW){fb_mode=FB_DETAIL;fb_dirty=true;return false;}
 if(fb_mode!=FB_LIST){fb_mode=FB_LIST;fb_choice=0;fb_dirty=true;return false;}
 if(fb_parent(fb_path)){fb_query[0]=0;(void)fb_load(0);return false;}
 if(!portable_file_browser_close()){fb_message("Storage release failed. Retry Back.");return false;}
 if(!fb_runtime->request_launch(FILE_BROWSER_RETURN_APP)){fb_message("Return failed. Retry Back.");return false;}
 return true;
}
static const char *const fb_options[]={"Find by name","Refresh folder","Show hidden","Go to root"};
static void fb_option(unsigned choice) {
 if(choice==0){strcpy(fb_editor,fb_query);fb_key_page=PWK_INITIAL_PAGE;fb_choice=0;fb_mode=FB_FILTER;fb_message("Done applies / Back cancels");}
 else {if(choice==2)fb_hidden=!fb_hidden;if(choice==3){strcpy(fb_path,"/");fb_query[0]=0;}fb_mode=FB_LIST;(void)fb_load(0);}
}
static void fb_preview_page(int direction) {
 unsigned step=fb_hex?64u:FB_PREVIEW_BYTES;
 if(direction>0){if(fb_offset+fb_preview_count>=fb_size || !fb_preview_count){fb_message("End of file");return;}fb_offset+=step;}
 else {if(!fb_offset){fb_message("Start of file");return;}fb_offset=fb_offset>step?fb_offset-step:0;}
 (void)fb_preview_read();
}
static void fb_draw(void) {
 portable_nova_begin();portable_nova_header(fb_mode==FB_FILTER?"FIND FILE":fb_mode==FB_PREVIEW?"PREVIEW":"FILES");
 if(fb_mode==FB_LIST) {
  portable_nova_button(184,5,44,38,"...",false);
  portable_nova_wrap(3,16,47,208,12,1,fb_path,NOVA_CYAN);
  for(unsigned i=0;i<fb_count;i++) {
   int y=62+(int)i*58;char size[32];fb_size_text(fb_rows[i].size,size,sizeof(size));
   portable_nova_button(16,y,208,54,"",i==fb_choice);
   portable_nova_wrap(1,26,y+4,188,15,2,fb_rows[i].name,NOVA_WHITE);
   portable_nova_text(3,26,y+35,188,fb_rows[i].is_directory?"FOLDER":size,NOVA_CYAN);
  }
  if(!fb_count)portable_nova_wrap(1,24,82,192,20,3,fb_status,NOVA_CAP);
  else {char status[64];snprintf(status,sizeof(status),"%u-%u of %u%s",fb_page*2+1,fb_page*2+fb_count,fb_matched,fb_query[0]?" matches":" items");portable_nova_text(3,18,175,204,status,NOVA_CAP);}
 } else if(fb_mode==FB_OPTIONS) {
  portable_nova_text(3,18,47,204,fb_label,NOVA_CAP);
  for(unsigned j=0;j<2;j++){unsigned i=fb_choice/2*2+j;portable_nova_row(16,62+(int)j*58,208,54,fb_options[i],i==2?(fb_hidden?"On":"Off"):"",i==fb_choice);}
 } else if(fb_mode==FB_FILTER) {
  size_t n=strlen(fb_editor);portable_nova_text(2,18,48,204,fb_editor+(n>23?n-23:0),NOVA_CYAN);
  for(unsigned i=0;i<PWK_COUNT;i++) {
   portable_watch_key_rect r;portable_watch_key_bounds(i,&r);unsigned ch=portable_watch_key_character(fb_key_page,i);char letter[]={(char)(ch?ch:' '),0};
   const char *label=i==PWK_PAGE?"ABC/#":i==PWK_DELETE?"DELETE":i==PWK_DONE?"DONE":ch==32?"_":letter;
   portable_nova_fill(r.x,r.y,r.w,r.h,i==fb_choice?NOVA_DIM:NOVA_LINE);portable_nova_text(2,r.x+5,r.y+7,r.w-10,label,NOVA_CYAN);
  }
  portable_nova_text(3,18,216,204,fb_status,NOVA_CAP);
 } else if(fb_mode==FB_DETAIL) {
  char size[32];fb_size_text(fb_size,size,sizeof(size));
  char name[65];snprintf(name,sizeof(name),"%.64s",fb_selected.name+fb_detail_offset);
  portable_nova_wrap(1,18,52,204,17,4,name,NOVA_WHITE);
  portable_nova_text(2,18,126,204,size,NOVA_CYAN);
  portable_nova_wrap(3,18,149,204,13,3,"Read-only storage. Files cannot change the installed system.",NOVA_CAP);
 } else {
  char line[64];snprintf(line,sizeof(line),"%s / byte %u",fb_hex?"HEX":"TEXT",(unsigned)fb_offset);portable_nova_text(3,18,47,204,line,NOVA_CYAN);
  unsigned pos=0;
  for(unsigned row=0;row<8 && pos<fb_preview_count;row++) {
   unsigned n=0;
   if(fb_hex){static const char digits[]="0123456789ABCDEF";for(unsigned col=0;col<8 && pos<fb_preview_count;col++){unsigned b=fb_preview[pos++];line[n++]=digits[b>>4];line[n++]=digits[b&15];line[n++]=' ';}}
   else {while(n<24 && pos<fb_preview_count){unsigned ch=fb_preview[pos++];line[n++]=(char)(ch>=32 && ch<=126?ch:ch=='\n'||ch=='\r'||ch=='\t'?' ':'.');}}
   line[n]=0;portable_nova_text(2,18,64+(int)row*14,204,line,NOVA_TEXT);
  }
  if(!fb_preview_count)portable_nova_wrap(1,24,89,192,20,3,fb_status,NOVA_CAP);
 }
 if(fb_mode!=FB_FILTER) {
  portable_nova_button(16,188,64,44,"Prev",false);
  portable_nova_button(88,188,64,44,fb_mode==FB_PREVIEW?(fb_hex?"Text":"Hex"):fb_mode==FB_DETAIL?"Read":"Open",false);
  portable_nova_button(160,188,64,44,"Next",false);
 }
 fb_app->present(false);fb_dirty=false;
}
static void fb_move(int direction) {
 if(fb_mode==FB_PREVIEW){fb_preview_page(direction);return;}
 if(fb_mode==FB_DETAIL){if(direction<0)fb_detail_offset=0;else if(strlen(fb_selected.name)>64)fb_detail_offset=64;fb_dirty=true;return;}
 unsigned count=fb_mode==FB_FILTER?PWK_COUNT:fb_mode==FB_OPTIONS?4:fb_count;
 if(!count)return;
 if(direction<0){if(fb_choice)fb_choice--;else if(fb_mode==FB_LIST)fb_page_previous();else fb_choice=count-1;}
 else {if(fb_choice+1<count)fb_choice++;else if(fb_mode==FB_LIST)fb_page_next();else fb_choice=0;}
 fb_dirty=true;
}
static void fb_activate(void) {
 if(fb_mode==FB_LIST)fb_activate_file(fb_choice);
 else if(fb_mode==FB_OPTIONS)fb_option(fb_choice);
 else if(fb_mode==FB_FILTER)fb_filter_key(fb_choice);
 else if(fb_mode==FB_DETAIL){fb_mode=FB_PREVIEW;fb_offset=0;(void)fb_preview_read();}
 else {fb_hex=!fb_hex;fb_offset=0;(void)fb_preview_read();}
 fb_dirty=true;
}
static bool fb_touch(int x,int y) {
 if(x<0 || y<0 || x>=240 || y>=240)return false;
 if(x<52 && y<44)return fb_back();
 if(fb_mode==FB_LIST && x>=184 && y<44){fb_mode=FB_OPTIONS;fb_choice=0;fb_dirty=true;return false;}
 if(fb_mode==FB_FILTER){int key=portable_watch_key_hit(x,y);if(key>=0){fb_choice=(unsigned)key;fb_filter_key(fb_choice);}return false;}
 if(y>=188 && y<232){if(x>=16 && x<80)fb_move(-1);else if(x>=88 && x<152)fb_activate();else if(x>=160 && x<224)fb_move(1);return false;}
 if(x>=16 && x<224 && (fb_mode==FB_LIST || fb_mode==FB_OPTIONS))for(unsigned row=0;row<2;row++)if(y>=62+(int)row*58 && y<116+(int)row*58){unsigned index=fb_mode==FB_OPTIONS?fb_choice/2*2+row:row;if(fb_mode==FB_OPTIONS || index<fb_count){fb_choice=index;fb_activate();}break;}
 return false;
}
static bool fb_open(void) {
 fb_app=t5_app_get_api(1);fb_runtime=risc_runtime_get_api(1);
 if(!fb_app || !fb_app->poll || !fb_app->millis || !fb_app->present || !fb_app->screen_width || !fb_app->screen_height || fb_app->screen_width()<240 || fb_app->screen_height()<240 || !fb_runtime || fb_runtime->api_version!=1 || fb_runtime->struct_size<RISC_RUNTIME_CAPABILITIES_V1_SIZE || !fb_runtime->acquire || !fb_runtime->release || !fb_runtime->request_launch || !fb_runtime->yield_ms || !fb_runtime->diagnostic)return false;
 memset(&fb_grant,0,sizeof(fb_grant));fb_volume=NULL;fb_retained_file=0;fb_count=fb_choice=fb_page=fb_mode=0;fb_offset=0;fb_hidden=fb_hex=fb_loaded=fb_contact=fb_drag=false;fb_armed=false;fb_dirty=true;fb_query[0]=fb_editor[0]=fb_label[0]=0;strcpy(fb_path,"/");
 if(fb_app->set_back_exits_app)fb_app->set_back_exits_app(false);
 (void)fb_load(0);return true;
}
void app_main(void) {
 if(!fb_open())return;
 for(;;) {
  if(fb_dirty)fb_draw();
  t5_app_input_t input={0};
  if(!fb_app->poll(&input,25)) {
#ifdef PORTABLE_ALARM_CLIENT
   if(portable_app_sleep_retained())return;
#endif
   if(!portable_file_browser_close()){fb_runtime->diagnostic("FILE_BROWSER cleanup-unconfirmed; invocation retained");
#if defined(PORTABLE_BLE_BROADCAST) || defined(PORTABLE_CONTEXTS_CLIENT)
    while(!portable_background_stop())fb_runtime->yield_ms(50);
#endif
    for(;;)fb_runtime->yield_ms(50);}
   return;
  }
  if(fb_app->struct_size>=offsetof(t5_app_api_v1,touch_contact)+sizeof(fb_app->touch_contact) && fb_app->touch_contact) {
   t5_app_contact_t contact={0};if(fb_app->touch_contact(&contact)) {
    int y=contact.y-(fb_app->screen_height()-240)/2;
    if(contact.down && !fb_contact){fb_contact_y=y;fb_drag=false;}
    if(contact.down && fb_contact && (y-fb_contact_y>36 || fb_contact_y-y>36)) {
     if(!fb_drag && fb_mode==FB_LIST && fb_contact_y>=62 && fb_contact_y<176) {
      if(y<fb_contact_y)fb_page_next();else fb_page_previous();fb_dirty=true;
     }
     fb_drag=true;
    }
    if(!contact.down && fb_contact && fb_drag)input.tapped=false;
    fb_contact=contact.down;
   }
  }
  /* A queued adapter handoff is terminal even while a folder/editor/preview
   * is open. Never reinterpret it as nested Back or replace its destination. */
  if(input.exit_requested) {
   if(!portable_file_browser_close()){fb_runtime->diagnostic("FILE_BROWSER handoff cleanup-unconfirmed; invocation retained");
#if defined(PORTABLE_BLE_BROADCAST) || defined(PORTABLE_CONTEXTS_CLIENT)
    while(!portable_background_stop())fb_runtime->yield_ms(50);
#endif
    for(;;)fb_runtime->yield_ms(50);}
   return;
  }
  if(!input.buttons && !input.tapped && !input.exit_requested){fb_armed=true;continue;}
  if(!fb_armed)continue;
  fb_armed=false;
  if(input.buttons&T5_APP_BUTTON_BACK){if(fb_back())return;continue;}
  if(input.tapped){if(fb_touch(input.touch_x-(fb_app->screen_width()-240)/2,input.touch_y-(fb_app->screen_height()-240)/2))return;continue;}
  if(input.buttons&(T5_APP_BUTTON_UP|T5_APP_BUTTON_LEFT))fb_move(-1);
  else if(input.buttons&(T5_APP_BUTTON_DOWN|T5_APP_BUTTON_RIGHT))fb_move(1);
  else if(input.buttons&T5_APP_BUTTON_CONFIRM)fb_activate();
 }
}

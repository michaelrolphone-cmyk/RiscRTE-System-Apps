/* Files-local native canvas. The existing controller owns every operation and
 * capability. Target identity is sealed with the presented screen generation. */
#include "PortableFilesNative.h"
typedef struct {int x,y,w,h;unsigned action,value;} fbn_target;
enum {FBN_BACK=1,FBN_MENU,FBN_PREV,FBN_NEXT,FBN_OPEN,FBN_ROW,FBN_CANCEL,FBN_UP,FBN_HERE,FBN_VOLUME,FBN_CONFIRM,FBN_EDIT,FBN_KEY,FBN_PEER,FBN_REJECT,FBN_MATCH,FBN_STOP,FBN_SETUP,FBN_DIRECT};
static fbn_target fbn_targets[40];
static unsigned fbn_count,fbn_rows,fbn_text_offset,fbn_text_end,fbn_text_length;
static unsigned fbn_last_mode,fbn_last_storage;
static uint64_t fbn_last_offset,fbn_input_generation;
static int fbn_width,fbn_height,fbn_pressed;
static bool fbn_contact,fbn_drag,fbn_can_drag,fbn_previous_tail;
static int fbn_contact_y;
static uint64_t fbn_identity;
static char fbn_text[1024];
static void fbn_hash(const void*,size_t);
static uint64_t fbn_frame_context;
static uint64_t fbn_context(void){
 uint64_t saved=fbn_identity;fbn_identity=UINT64_C(1469598103934665603);
 fbn_hash(&fb_mode,sizeof(fb_mode));fbn_hash(&fb_storage_index,sizeof(fb_storage_index));fbn_hash(fb_path,strlen(fb_path));fbn_hash(fb_file_path,strlen(fb_file_path));fbn_hash(fb_selected.name,strlen(fb_selected.name));fbn_hash(&fb_offset,sizeof(fb_offset));fbn_hash(&fb_count,sizeof(fb_count));
 for(unsigned i=0;i<fb_count;i++){fbn_hash(fb_rows[i].name,strlen(fb_rows[i].name));fbn_hash(&fb_rows[i].is_directory,sizeof(fb_rows[i].is_directory));}
 fbn_hash(&fb_handler_offset,sizeof(fb_handler_offset));
#ifdef PORTABLE_FILE_SHARING
 fbn_hash(&fsh_phase,sizeof(fsh_phase));fbn_hash(&fsh_status.expires_ms,sizeof(fsh_status.expires_ms));
#ifdef PORTABLE_FILE_PEER
 fbn_hash(&fpeer_mode,sizeof(fpeer_mode));fbn_hash(&fpeer_status.state,sizeof(fpeer_status.state));fbn_hash(&fpeer_status.pairing_generation,sizeof(fpeer_status.pairing_generation));fbn_hash(&fpeer_status.pairing_number,sizeof(fpeer_status.pairing_number));
 if(fpeer_mode&&fpeer_status.state==PEER_DISCOVERING)for(unsigned i=0;i<fpeer_status.count;i++){portable_peer_row row;if(portable_peer_row_get(fpeer,i,&row))fbn_hash(&row.peer_id,sizeof(row.peer_id));}
#endif
#ifdef PORTABLE_FILE_SETUP
 fbn_hash(&fsh_setup_screen,sizeof(fsh_setup_screen));fbn_hash(&fsh_setup_status.pairing_generation,sizeof(fsh_setup_status.pairing_generation));fbn_hash(&fsh_setup_status.pairing_number,sizeof(fsh_setup_status.pairing_number));
#endif
#endif
 uint64_t result=fbn_identity;fbn_identity=saved;return result;
}
static void fbn_hash(const void *data,size_t n){const unsigned char*p=data;while(n--){fbn_identity^=*p++;fbn_identity*=UINT64_C(1099511628211);}}
static bool fbn_inside(fbn_target r,int x,int y){return x>=r.x&&y>=r.y&&x-r.x<r.w&&y-r.y<r.h;}
static int fbn_hit(int x,int y){for(unsigned i=0;i<fbn_count;i++)if(fbn_inside(fbn_targets[i],x,y))return (int)i;return -1;}
static void fbn_label(int x,int y,int w,int h,const char*s,uint32_t color){
 unsigned face=h>=32&&w>=200?1:h>=20?2:6;
 if(w<80&&portable_nova_measure(face,s)>w-8)face=6;
 int line=face==1?20:face==2?16:10;
 char fit[160];snprintf(fit,sizeof(fit),"%s",s);size_t n=strlen(fit);
 while(n&&portable_nova_measure(face,fit)>w-8){fit[--n]=0;}
 if(n<strlen(s)&&n>=3){fit[n-3]=fit[n-2]=fit[n-1]='.';}
 if(h>=line)portable_nova_native_text(face,x+4,y+(h-line)/2,w-8,fit,color);
}
static void fbn_button(int x,int y,int w,int h,const char*s,unsigned action,unsigned value,bool selected){
 if(fbn_count>=sizeof(fbn_targets)/sizeof(fbn_targets[0]))return;
 fbn_target r={x,y,w,h,action,value};fbn_targets[fbn_count++]=r;
 fbn_hash(&r,sizeof(r));fbn_hash(s,strlen(s));
 portable_nova_native_round(x,y,w,h,4,selected?NOVA_CYAN:NOVA_DIM);
 portable_nova_native_round(x+1,y+1,w-2,h-2,3,0x031115u);fbn_label(x,y,w,h,s,selected?NOVA_WHITE:NOVA_TEXT);
}
static void fbn_footer(unsigned i,unsigned n,const char*s,unsigned action,unsigned value){int x=4+(fbn_width-8)*(int)i/(int)n,xx=4+(fbn_width-8)*(int)(i+1)/(int)n;fbn_button(x,fbn_height-46,xx-x-2,42,s,action,value,false);}
static void fbn_navigation(const char*middle){fbn_footer(0,3,"Prev",FBN_PREV,0);fbn_footer(1,3,middle,FBN_OPEN,0);fbn_footer(2,3,"Next",FBN_NEXT,0);}
static unsigned fbn_page_end(unsigned pos,bool draw){
 unsigned face=fbn_width>=240?2:6;int line_height=face==2?16:10;
 for(int y=58;y+line_height<=fbn_height-50&&pos<fbn_text_length;y+=line_height+3){
  char line[192];unsigned n=0,start=pos;while(pos<fbn_text_length&&n<sizeof(line)-1){char c=fbn_text[pos];if(c=='\n'){pos++;break;}line[n]=c;line[n+1]=0;if(portable_nova_measure(face,line)>fbn_width-16){line[n]=0;break;}n++;pos++;}
  if(fb_mode==FB_PREVIEW&&fb_hex&&fb_preview_count){unsigned remainder=n%3;n-=remainder;pos-=remainder;}
  line[n]=0;
  if(pos==start&&pos<fbn_text_length)pos++; /* Bounded even for unsupported glyphs. */
  if(draw)portable_nova_native_text(face,8,y,fbn_width-16,line,NOVA_TEXT);
 }return pos;
}
static void fbn_body(void){
 fbn_text_length=(unsigned)strlen(fbn_text);if(fbn_text_offset>=fbn_text_length)fbn_text_offset=0;
 if(fbn_previous_tail){
  if(fb_mode==FB_PREVIEW&&fb_preview_count)for(unsigned pos=0;pos<fbn_text_length;){fbn_text_offset=pos;unsigned next=fbn_page_end(pos,false);if(next<=pos)break;pos=next;}
  fbn_previous_tail=false;
 }
 fbn_text_end=fbn_page_end(fbn_text_offset,true);
}
static unsigned fbn_row_start(unsigned choice){return choice/fbn_rows*fbn_rows;}
static void fbn_row(unsigned slot,const char*label,unsigned action,unsigned value,bool selected){int usable=fbn_height-110;int y=60+usable*(int)slot/(int)fbn_rows,yy=60+usable*(int)(slot+1)/(int)fbn_rows;fbn_button(4,y,fbn_width-8,yy-y-3,label,action,value,selected);}
static void fbn_reset_view(void){fbn_text_offset=0;fbn_previous_tail=false;fbn_last_mode=UINT32_MAX;fbn_contact=false;fbn_pressed=-1;portable_files_native_begin();}
static void fbn_draw(void){
 if(fb_paper)return;
#ifdef PORTABLE_FILE_BROWSER_SHARED_INPUT
 if(fb_text_active)return;
#endif
#ifdef PORTABLE_FILE_SETUP
 if(portable_file_browser_cleanup_only())return;
#endif
 if(fbn_last_mode!=fb_mode||fbn_last_storage!=fb_storage_index||fbn_last_offset!=fb_offset){fbn_text_offset=0;fbn_last_mode=fb_mode;fbn_last_storage=fb_storage_index;fbn_last_offset=fb_offset;}
 fb_dirty=false;fbn_count=0;fbn_identity=UINT64_C(1469598103934665603);fbn_text[0]=0;fbn_text_end=fbn_text_length=0;
 fbn_frame_context=fbn_context();fbn_hash(&fbn_frame_context,sizeof(fbn_frame_context));
 fbn_hash(&fb_mode,sizeof(fb_mode));fbn_hash(&fb_storage_index,sizeof(fb_storage_index));fbn_hash(fb_path,strlen(fb_path));fbn_hash(&fb_offset,sizeof(fb_offset));
 portable_nova_begin();portable_nova_native_fill(0,0,fbn_width,fbn_height,0x010708u);
 fbn_button(4,4,42,42,"<",FBN_BACK,0,false);
 const char*title=fb_mode==FB_PREVIEW?"PREVIEW":fb_mode==FB_OPTIONS?"OPTIONS":fb_mode==FB_RENAME?"RENAME":fb_mode==FB_FILTER?"FIND":"FILES";
 fbn_label(48,4,fbn_width-96,42,title,NOVA_CYAN);
#ifdef PORTABLE_FILE_SHARING
 if(fb_mode==FB_SHARING){
#ifdef PORTABLE_FILE_PEER
  if(fpeer_mode){
   const portable_peer_status frame=fpeer_status;fpeer_pair_generation=0;
   if(frame.state==PEER_PAIRING){char number[16];snprintf(number,sizeof(number),"%06u",frame.pairing_number);portable_nova_native_fill(48,4,fbn_width-52,42,0x010708u);fbn_label(48,4,fbn_width-52,42,number,NOVA_WHITE);snprintf(fbn_text,sizeof(fbn_text),"Compare both numbers. Match only if identical. HTTP files are unencrypted.");fbn_body();fbn_footer(0,3,"Reject",FBN_PEER,0);fbn_footer(1,3,"Match",FBN_PEER,1);fbn_footer(2,3,"Stop",FBN_PEER,2);fbn_hash(&frame.pairing_generation,sizeof(frame.pairing_generation));fbn_hash(&frame.pairing_number,sizeof(frame.pairing_number));}
   else {unsigned count=fpeer_count(),first=fbn_row_start(fpeer_choice);char label[160];for(unsigned j=0;j<fbn_rows&&first+j<count;j++)fbn_row(j,fpeer_label(first+j,label,sizeof(label)),FBN_PEER,first+j,first+j==fpeer_choice);fbn_navigation("Open");}
   portable_files_native_layout(fbn_identity);portable_files_native_present(false);if(frame.state==PEER_PAIRING){fpeer_pair_generation=frame.pairing_generation;fpeer_pair_number=frame.pairing_number;}return;
  }
#endif
#ifdef PORTABLE_FILE_SETUP
  if(fsh_setup_screen){const portable_file_setup_status frame=fsh_setup_status;bool pairing=frame.state==PORTABLE_FILE_SETUP_STATE_ACTIVE&&frame.provider_state==RISC_SETUP_PAIR_CONFIRM&&frame.pairing_generation;
   fsh_setup_display_generation=fsh_setup_pending_generation=0;
   if(pairing){char number[16];snprintf(number,sizeof(number),"%06u",frame.pairing_number);portable_nova_native_fill(48,4,fbn_width-52,42,0x010708u);fbn_label(48,4,fbn_width-52,42,number,NOVA_WHITE);snprintf(fbn_text,sizeof(fbn_text),"Compare both numbers. HTTP files are unencrypted.");fbn_footer(0,2,"Reject",FBN_REJECT,0);fbn_footer(1,2,"Match",FBN_MATCH,0);fbn_hash(&frame.pairing_generation,sizeof(frame.pairing_generation));fbn_hash(&frame.pairing_number,sizeof(frame.pairing_number));}
   else{snprintf(fbn_text,sizeof(fbn_text),"%s\n%s\nHTTP files are unencrypted.",fsh_setup_label(),fsh_message);fbn_navigation("Back");}
   fbn_body();portable_files_native_layout(fbn_identity);portable_files_native_present(false);if(pairing){fsh_setup_pending_generation=frame.pairing_generation;fsh_setup_pending_number=frame.pairing_number;}fsh_setup_display_sync();return;
  }
#endif
  bool ready=fsh_phase==FSH_ACTIVE&&(fsh_status.state==PORTABLE_FILE_SHARING_LISTENING||fsh_status.state==PORTABLE_FILE_SHARING_SERVING);
  if(ready)snprintf(fbn_text,sizeof(fbn_text),"HTTP unencrypted.\n%s\nUser: %s\nPassword: %s\n%u seconds left / %u requests\n%s",fsh_url,fsh_status.username,fsh_status.password,fsh_status.expires_ms>fsh_now?(unsigned)(fsh_status.expires_ms-fsh_now)/1000u:0,fsh_status.completed_requests,fsh_message);
  else snprintf(fbn_text,sizeof(fbn_text),"HTTP unencrypted.\n%s\n%s",fsh_phase==FSH_OFF?"Share files for five minutes. Confirm setup on both devices.":"Preparing session. Back cancels.",fsh_message);
#ifdef PORTABLE_FILE_SETUP
  if(ready)fbn_button(fbn_width-46,4,42,42,"Setup",FBN_SETUP,0,false);
  else if(fsh_phase==FSH_OFF)fbn_button(fbn_width-46,4,42,42,fsh_direct?"Direct":"LAN",FBN_DIRECT,0,false);
#endif
  fbn_body();fbn_navigation(fsh_phase==FSH_OFF?"Share":"Stop");goto present;
 }
#endif
 if(fb_mode==FB_LIST||fb_mode==FB_OPTIONS||fb_mode==FB_ACTIONS||fb_mode==FB_HANDLERS||fb_mode==FB_DESTINATION){
  fbn_label(4,46,fbn_width-8,12,fb_feedback?fb_status:fb_path,NOVA_CAP);
  unsigned count=fb_mode==FB_OPTIONS?fb_option_count():fb_mode==FB_ACTIONS?FBX_ACTION_COUNT:fb_mode==FB_HANDLERS?fb_handler_count-fb_handler_offset:fb_count;
  unsigned first=(fb_mode==FB_OPTIONS||fb_mode==FB_ACTIONS)?fbn_row_start(fb_choice):0;
  for(unsigned j=0;j<fbn_rows&&first+j<count;j++){unsigned at=first+j;const char*label=fb_mode==FB_OPTIONS?fb_options[at]:fb_mode==FB_ACTIONS?fbx_labels[at]:fb_mode==FB_HANDLERS?fb_handlers[at].display_name:fb_rows[at].name;char row_label[320];if(fb_mode==FB_LIST||fb_mode==FB_DESTINATION)snprintf(row_label,sizeof(row_label),"%s%s",fb_rows[at].is_directory?"[DIR] ":"",label);else if(fb_mode==FB_OPTIONS&&at==2)snprintf(row_label,sizeof(row_label),"%s: %s",label,fb_hidden?"On":"Off");else snprintf(row_label,sizeof(row_label),"%s",label);fbn_row(j,row_label,FBN_ROW,at,at==fb_choice);}
  if(!count){snprintf(fbn_text,sizeof(fbn_text),"%s",fb_status);fbn_body();}
  if(fb_mode==FB_LIST)fbn_button(fbn_width-46,4,42,42,"...",FBN_MENU,0,false);
  if(fb_mode==FB_DESTINATION){if(!fb_moving)fbn_button(fbn_width-46,4,42,42,"VOL",FBN_VOLUME,0,false);fbn_footer(0,3,"Up",FBN_UP,0);fbn_footer(1,3,"Here",FBN_HERE,0);fbn_footer(2,3,"Next",FBN_NEXT,0);}
  else fbn_navigation("Open");
 }else if(fb_mode==FB_FILTER||fb_mode==FB_RENAME){
#ifdef PORTABLE_FILE_BROWSER_SHARED_INPUT
  snprintf(fbn_text,sizeof(fbn_text),"%s\n%s",fb_editor,fb_status);fbn_body();fbn_footer(0,1,"Edit",FBN_EDIT,0);
#else
  fbn_label(4,47,fbn_width-8,12,fb_editor,NOVA_TEXT);
  for(unsigned i=0;i<PWK_COUNT;i++){int col=(int)i%5,row=(int)i/5,x=4+(fbn_width-8)*col/5,xx=4+(fbn_width-8)*(col+1)/5,y=62+(fbn_height-66)*row/7,yy=62+(fbn_height-66)*(row+1)/7;unsigned ch=portable_watch_key_character(fb_key_page,i);char key[2]={(char)ch,0};const char*label=i==PWK_PAGE?"ABC/#":i==PWK_DELETE?"Del":i==PWK_DONE?"Done":ch==32?"_":key;fbn_button(x,y,xx-x-2,yy-y-2,label,FBN_KEY,i,i==fb_choice);}
#endif
 }else if(fb_mode==FB_DELETE||fb_mode==FB_REPLACE){fbn_label(4,46,fbn_width-8,12,fb_selected.name,NOVA_CAP);snprintf(fbn_text,sizeof(fbn_text),"%s",fb_mode==FB_DELETE?"Permanently delete?\nCannot undo.":"Replace existing file?\nThis overwrites it.");fbn_body();fbn_footer(0,2,"Cancel",FBN_CONFIRM,0);fbn_footer(1,2,fb_mode==FB_DELETE?"Delete":"Replace",FBN_CONFIRM,1);
 }else if(fb_mode==FB_NOTICE){snprintf(fbn_text,sizeof(fbn_text),"%s",fb_status);fbn_body();fbn_navigation("Back");
 }else if(fb_mode==FB_DETAIL){char size[32];fb_size_text(fb_size,size,sizeof(size));snprintf(fbn_text,sizeof(fbn_text),"%s\n%s\nRead-only preview. Installed system files cannot be changed.",fb_selected.name,size);fbn_body();fbn_navigation("Read");
 }else if(fb_mode==FB_PREVIEW){unsigned n=0;static const char hex[]="0123456789ABCDEF";for(unsigned i=0;i<fb_preview_count&&n+4<sizeof(fbn_text);i++){unsigned c=fb_preview[i];if(fb_hex){fbn_text[n++]=hex[c>>4];fbn_text[n++]=hex[c&15];fbn_text[n++]=' ';}else fbn_text[n++]=(char)(c>=32&&c<=126?c:c=='\n'?'\n':c=='\r'||c=='\t'?' ':'.');}fbn_text[n]=0;if(!n)snprintf(fbn_text,sizeof(fbn_text),"%s",fb_status);fbn_body();fbn_navigation(fb_hex?"Text":"Hex");}
#ifdef PORTABLE_FILE_SHARING
present:
#endif
 fbn_hash(&fbn_text_offset,sizeof(fbn_text_offset));portable_files_native_layout(fbn_identity);portable_files_native_present(false);
}
static void fbn_page(int direction){
 if(fbn_text_length){
  if(direction>0&&fbn_text_end<fbn_text_length){fbn_text_offset=fbn_text_end;fb_dirty=true;return;}
  if(direction<0&&fbn_text_offset){unsigned previous=0;for(unsigned pos=0;pos<fbn_text_offset;){unsigned next=fbn_page_end(pos,false);if(next>=fbn_text_offset||next<=pos)break;previous=next;pos=next;}fbn_text_offset=previous;fb_dirty=true;return;}
  if(fb_mode!=FB_PREVIEW){fb_dirty=true;return;}
 }
 if(fb_mode==FB_PREVIEW){fbn_previous_tail=direction<0&&fb_offset>0;fb_preview_page(direction);}
 else if(fb_mode==FB_DESTINATION){if(direction>0)fb_page_next();else fb_page_previous();}
 else fb_move(direction);
}
static bool fbn_action(unsigned action,unsigned value){
#ifdef PORTABLE_FILE_BROWSER_SOURCE_HANDOFF
 if(fsource_busy())return action==FBN_BACK?fb_back():false;
#endif
#ifdef PORTABLE_FILE_PEER
 if(fbm_mounted&&fbm_busy())return action==FBN_BACK?fb_back():false;
#endif
#ifdef PORTABLE_FILE_SETUP
 if(portable_file_browser_cleanup_only())return false;
#endif
 if(action==FBN_BACK){
#ifdef PORTABLE_FILE_SETUP
  if(fb_mode==FB_SHARING&&fsh_setup_screen)return fsh_setup_touch(20,210);
#endif
  return fb_back();
 }
 if(action==FBN_PREV||action==FBN_NEXT){fbn_page(action==FBN_PREV?-1:1);return false;}
#ifdef PORTABLE_FILE_PEER
 if(action==FBN_PEER){fpeer_choice=value;fpeer_action();return false;}
#endif
#ifdef PORTABLE_FILE_SETUP
 if(action==FBN_REJECT||action==FBN_MATCH){fsh_setup_choice(action==FBN_MATCH);return false;}
 if(action==FBN_SETUP){fsh_setup_start();return false;}
 if(action==FBN_DIRECT){if(fsh_phase==FSH_OFF)fsh_direct=!fsh_direct;fb_dirty=true;return false;}
#endif
 if(action==FBN_OPEN){
  if(fb_mode==FB_NOTICE)return fb_back();
#ifdef PORTABLE_FILE_SETUP
  if(fb_mode==FB_SHARING&&fsh_setup_screen)return fsh_setup_touch(20,210);
#endif
#ifdef PORTABLE_FILE_SHARING
  if(fb_mode==FB_SHARING){
#ifdef PORTABLE_FILE_PEER
   if(fpeer_mode)fpeer_action();else
#endif
   fsh_action();
  }else
#endif
  {fb_activate();}
  fbn_text_offset=0;
 }else if(action==FBN_MENU){fb_list_choice=fb_choice;fb_mode=FB_OPTIONS;fb_choice=0;}
 else if(action==FBN_ROW){fb_choice=value;fb_activate();}
 else if(action==FBN_UP){if(fb_parent(fb_path))(void)fb_load(0);}
 else if(action==FBN_HERE)fbx_commit_destination();
 else if(action==FBN_VOLUME){if(fbx_swap()){strcpy(fb_path,"/");(void)fb_load(0);}}
 else if(action==FBN_CONFIRM){fb_choice=value;fbx_activate();}
#ifdef PORTABLE_FILE_BROWSER_SHARED_INPUT
 else if(action==FBN_EDIT)(void)fb_text_begin(fb_mode,fb_mode==FB_FILTER?"Find file":"Edit name");
#else
 else if(action==FBN_KEY){fb_choice=value;fb_filter_key(value);}
#endif
 fb_dirty=true;return false;
}
static bool fbn_touch(int x,int y){if(fbn_context()!=fbn_frame_context){fb_dirty=true;return false;}int hit=fbn_hit(x,y);if(hit<0)return false;fbn_target r=fbn_targets[hit];return fbn_action(r.action,r.value);}
static void fbn_input(t5_app_input_t*in){
 uint64_t generation=0;bool ready=portable_files_native_ready(&generation,NULL);
 if(generation!=fbn_input_generation){fbn_contact=false;fbn_pressed=-1;fbn_input_generation=generation;}
 t5_app_contact_t contact={0};bool ok=fb_app->touch_contact&&fb_app->touch_contact(&contact);
 if(!ready||!ok){fbn_contact=false;fbn_pressed=-1;in->tapped=false;return;}
 if(contact.down){int target=fbn_hit(contact.x,contact.y);if(!fbn_contact){fbn_contact=true;fbn_pressed=target;fbn_drag=false;fbn_contact_y=contact.y;fbn_can_drag=fb_mode==FB_LIST&&target>=0&&fbn_targets[target].action==FBN_ROW;}else{if(target!=fbn_pressed)fbn_pressed=-1;if(!fbn_drag&&fbn_can_drag&&(contact.y-fbn_contact_y>36||fbn_contact_y-contact.y>36)){fbn_drag=true;fbn_pressed=-1;if(contact.y<fbn_contact_y)fb_page_next();else fb_page_previous();fb_dirty=true;}}}
 if(in->tapped){int target=fbn_hit(in->touch_x,in->touch_y);if(!fbn_contact||target!=fbn_pressed)in->tapped=false;fbn_contact=false;fbn_pressed=-1;}
 else if(!contact.down){fbn_contact=false;fbn_pressed=-1;}
}

/* Presentation snapshots support raster diagnostics only. Input pins current
 * logical identity independently, then validates it again before activation. */
typedef struct {
 unsigned generation,mode,start,count,selected,origin;
 int q8;
 union {risc_storage_dirent_v1 rows[FB_MAX_PAGE_ROWS];t5_file_handler_t handlers[FB_MAX_PAGE_ROWS];} items;
} fbs_image;
static fbs_image fbs_submitted,fbs_completed;
static unsigned fbs_hit_generation,fbs_keyboard_page;
static unsigned fbs_hit_row;
static bool fbs_hit_has_row;
static union {risc_storage_dirent_v1 row;t5_file_handler_t handler;} fbs_hit_item;
static void fbs_visible(void) {if(paper_frame_idle())fbs_completed=fbs_submitted;}
static unsigned fbs_count(void) {
 return fbs_directory_mode()?(fb_loaded?fb_matched:0):fb_mode==FB_OPTIONS?fb_option_count():fb_mode==FB_ACTIONS?7:fb_mode==FB_HANDLERS?fb_handler_count:0;
}
/* Rebase a finite pixel coordinate window without imposing a folder-size
 * limit. Absolute row identity remains unsigned and travels with each image. */
static void fbs_configure(void) {
 unsigned count=fbs_count();
 unsigned max_origin=count>FBS_COORDINATE_ROWS?count-FBS_COORDINATE_ROWS:0;
 if(fbs_origin>max_origin)fbs_origin=max_origin;
 unsigned local=count-fbs_origin;if(local>FBS_COORDINATE_ROWS)local=FBS_COORDINATE_ROWS;
 portable_scroll_configure(&fbs_scroll,(portable_scroll_viewport){32,FBS_TOP,fbp_w()-64,fbp_h()-168-FBS_TOP},FBS_ROW,(int)local);
}
static void fbs_reveal(unsigned row) {
 unsigned count=fbs_count();if(row>=count)return;
 if(row<fbs_origin||row-fbs_origin>=FBS_COORDINATE_ROWS){
  unsigned max_origin=count>FBS_COORDINATE_ROWS?count-FBS_COORDINATE_ROWS:0;
  fbs_origin=row>FBS_COORDINATE_ROWS/2?row-FBS_COORDINATE_ROWS/2:0;
  if(fbs_origin>max_origin)fbs_origin=max_origin;
  fbs_scroll.position_q8=0;fbs_configure();
 }
 portable_scroll_reveal(&fbs_scroll,(int)(row-fbs_origin));
}
static void fbs_slide_coordinates(void) {
 unsigned count=fbs_count();if(count<=FBS_COORDINATE_ROWS)return;
 unsigned first=(unsigned)portable_scroll_offset(&fbs_scroll)/FBS_ROW;
 unsigned next=fbs_origin,max_origin=count-FBS_COORDINATE_ROWS;
 if(first>FBS_COORDINATE_ROWS*3/4&&next<max_origin){unsigned step=max_origin-next;if(step>FBS_COORDINATE_ROWS/2)step=FBS_COORDINATE_ROWS/2;next+=step;}
 else if(first<FBS_COORDINATE_ROWS/4&&next){unsigned step=next;if(step>FBS_COORDINATE_ROWS/2)step=FBS_COORDINATE_ROWS/2;next-=step;}
 int shift=next>=fbs_origin?-(int)(next-fbs_origin)*FBS_ROW*256:(int)(fbs_origin-next)*FBS_ROW*256;
 fbs_origin=next;fbs_scroll.position_q8+=shift;fbs_scroll.start_q8+=shift;fbs_configure();
}
static void fbs_prepare(void) {
 bool changed=fbs_mode!=fb_mode;
 if(!changed&&(fb_mode==FB_FILTER||fb_mode==FB_RENAME)&&fbs_keyboard_page!=fbp_key_page){
  portable_file_browser_scroll_interrupt();++fbs_generation;
 }
 fbs_keyboard_page=fbp_key_page;
 if(changed){
  portable_file_browser_scroll_interrupt();++fbs_generation;fbs_mode=fb_mode;fbs_scroll.position_q8=0;fbs_origin=0;
  if(fbs_directory_mode())fbs_selected=fbs_directory_selected;
  else fbs_selected=fb_mode==FB_HANDLERS?fb_handler_offset+fb_choice:fb_choice;
 }
 fbs_configure();
 if(changed)fbs_reveal(fbs_selected);
}
static void fbs_record(void) {
 fbs_submitted=(fbs_image){.generation=fbs_generation,.mode=fb_mode,.q8=fbs_scroll.position_q8,.selected=fbs_selected,.origin=fbs_origin};
 if(fbs_directory_mode()){
  fbs_submitted.start=fbs_cache_start;fbs_submitted.count=fb_count;
  memcpy(fbs_submitted.items.rows,fb_rows,fb_count*sizeof(*fb_rows));
 }else if(fb_mode==FB_HANDLERS){
  fbs_submitted.start=fb_handler_offset;fbs_submitted.count=fb_handler_count-fb_handler_offset;
  if(fbs_submitted.count>FB_MAX_PAGE_ROWS)fbs_submitted.count=FB_MAX_PAGE_ROWS;
  memcpy(fbs_submitted.items.handlers,fb_handlers,fbs_submitted.count*sizeof(*fb_handlers));
 }
 fbs_visible();
}
static bool fbs_handlers_for(unsigned first) {
 unsigned needed=(unsigned)fbs_scroll.view.height/FBS_ROW+2;
 unsigned end=first+needed;if(end>fb_handler_count)end=fb_handler_count;
 if(first>=fb_handler_offset&&end<=fb_handler_offset+FB_MAX_PAGE_ROWS)return true;
 fb_handler_offset=first;
 if(!fbx_handler_page())return false;
 fb_choice=fbs_selected>=first&&fbs_selected<first+FB_MAX_PAGE_ROWS?fbs_selected-first:0;
 return true;
}
static void fbs_draw(void) {
 unsigned first=fbs_origin+(unsigned)portable_scroll_offset(&fbs_scroll)/FBS_ROW;
 if(fbs_directory_mode()&&!fbs_cache_for(first)){if(!fb_loaded){first=0;fbs_prepare();}else return;}
 if(fb_mode==FB_HANDLERS&&!fbs_handlers_for(first))return;
 fb_paper->begin();
 const char *title=fb_mode==FB_DESTINATION?(fb_moving?"MOVE TO":"COPY TO"):fb_mode==FB_HANDLERS?"OPEN WITH":fb_mode==FB_ACTIONS?"FILE ACTIONS":fb_mode==FB_OPTIONS?"FILE OPTIONS":"FILES";
 fbp_text(32,28,fbp_w()-152,title,true,true,false);
 fbp_text(32,72,fbp_w()-160,(fb_mode==FB_ACTIONS||fb_mode==FB_HANDLERS)?fb_selected.name:fb_path,false,true,true);
 fb_app->fill_rect(32,106,fbp_w()-64,3,true);
 if(fb_mode==FB_LIST)fbp_button(fbp_w()-120,8,88,88,"","solid:f141",false);
 if(fb_mode==FB_DESTINATION&&!fb_moving)fbp_button(fbp_w()-120,8,88,88,"VOL",NULL,false);
 portable_file_browser_scroll_clip(&fbs_scroll.view);
 unsigned rows=(unsigned)fbs_scroll.view.height/FBS_ROW+2,total=fbs_count();
 for(unsigned j=0;j<rows&&j<total-first;++j){
  unsigned index=first+j;int y=portable_scroll_row_y(&fbs_scroll,(int)(index-fbs_origin));bool chosen=index==fbs_selected;
  if(chosen)fb_app->fill_rect(32,y,fbp_w()-64,FBS_ROW,true);
  if(fbs_directory_mode()){
   if(index<fbs_cache_start||index>=fbs_cache_start+fb_count)continue;
   risc_storage_dirent_v1 *row=&fb_rows[index-fbs_cache_start];char size[32];fb_size_text(row->size,size,sizeof(size));
   fbp_icon(48,y+22,row->is_directory?"solid:f07c":"solid:f15b",32,!chosen);
   fbp_text(96,y+10,fbp_w()-(fb_manage&&fb_mode==FB_LIST?200:144),row->name,false,!chosen,true);
   if(fb_manage&&fb_mode==FB_LIST)fbp_icon(fbp_w()-80,y+26,"solid:f141",24,!chosen);
   fbp_text(96,y+42,fbp_w()-144,row->is_directory?"FOLDER":size,false,!chosen,false);
  }else {
   const char *label=fb_mode==FB_ACTIONS?fbx_labels[index]:fb_mode==FB_OPTIONS?fb_options[index]:fb_handlers[index-fb_handler_offset].display_name;
   fbp_text(48,y+14,fbp_w()-112,label,false,!chosen,fb_mode==FB_HANDLERS);
   if(fb_mode==FB_OPTIONS&&index==2)fbp_text(48,y+44,fbp_w()-112,fb_hidden?"ON":"OFF",false,!chosen,false);
   fbp_icon(fbp_w()-72,y+30,"solid:f054",24,!chosen);
  }
  fb_app->fill_rect(32,y+FBS_ROW-3,fbp_w()-64,3,true);
 }
 portable_file_browser_scroll_clip(NULL);
 if(fbs_scroll.limit){
  uint64_t content=(uint64_t)total*FBS_ROW,offset=(uint64_t)fbs_origin*FBS_ROW+(unsigned)portable_scroll_offset(&fbs_scroll);
  unsigned square=(unsigned)fbs_scroll.view.height*(unsigned)fbs_scroll.view.height;
  int bar=total<=square/(24u*FBS_ROW)?(int)(square/(total*FBS_ROW)):24;
  uint64_t range=content-(unsigned)fbs_scroll.view.height;
  /* Preserve subpixel thumb accuracy without introducing a 64-bit divider
   * from the target toolchain's non-relocatable helper objects. */
  while(range>1048575u){range>>=1;offset>>=1;}
  int y=FBS_TOP+(fbs_scroll.view.height-bar)*(unsigned)offset/(unsigned)range;
  fb_app->fill_rect(fbp_w()-24,y,4,bar,true);
 }
 if(!total&&fbs_directory_mode())fbp_wrap(32,150,fbp_w()-64,4,fb_status,true);
 else fbp_text(32,fbp_h()-146,fbp_w()-64,fb_feedback?fb_status:fb_mode==FB_DESTINATION?"Choose folder; HERE confirms":"Swipe to scroll",false,true,false);
 unsigned columns=fb_mode==FB_DESTINATION?3:2;int w=(fbp_w()-64-8*(int)(columns-1))/(int)columns;
 for(unsigned i=0;i<columns;++i)fbp_button(32+(int)i*(w+8),fbp_h()-112,w,88,i==0?(fb_mode==FB_DESTINATION?"CANCEL":"BACK"):fb_mode==FB_DESTINATION?(i==1?"UP":"HERE"):"OPEN",NULL,i==columns-1);
 fb_app->present(fbp_clean||fbp_last_mode!=fb_mode);fbp_clean=false;fbp_last_mode=fb_mode;fb_dirty=false;fbs_record();
}
static bool fbs_choose(unsigned row) {
 if(row>=fbs_count())return false;
 if(fbs_directory_mode()){
  if(!fbs_cache_for(row)||row<fbs_cache_start||row>=fbs_cache_start+fb_count)return false;
  fb_choice=row-fbs_cache_start;
 }else if(fb_mode==FB_HANDLERS){if(!fbs_handlers_for(row))return false;fb_choice=row-fb_handler_offset;}
 else fb_choice=row;
 fbs_selected=row;if(fbs_directory_mode())fbs_directory_selected=row;return true;
}
static bool fbs_move(int direction) {
 fbs_prepare();if(!fbs_list_mode())return false;
 unsigned count=fbs_count();if(!count)return true;
 unsigned next=fbs_selected;
 if(direction<0){if(next)--next;else if(!fbs_directory_mode())next=count-1;}
 else if(next+1<count)++next;else if(!fbs_directory_mode())next=0;
 if(fbs_choose(next)){fbs_reveal(next);fb_dirty=true;}
 return true;
}
static bool fbs_confirm_ready(void) {
 fbs_prepare();
 if(!fbs_list_mode())return true;
 /* Resolve the logical selection/cache now. A hidden or unpainted selection
  * does not consume Confirm merely to wait for another display frame. */
 if(!fbs_choose(fbs_selected))return false;
 fbs_reveal(fbs_selected);fb_dirty=true;return true;
}
static bool fbs_touch(int x,int y) {
 if(x<32||x>=fbp_w()-32)return false;
 if(fb_mode==FB_LIST&&x>=fbp_w()-120&&y>=8&&y<96){fb_list_choice=fb_choice;fb_mode=FB_OPTIONS;fb_choice=0;fb_dirty=true;return false;}
 if(fb_mode==FB_DESTINATION&&!fb_moving&&x>=fbp_w()-120&&y>=8&&y<96){if(fbx_swap()){strcpy(fb_path,"/");(void)fb_load(0);}return false;}
 if(y>=fbp_h()-112&&y<fbp_h()-24){
  unsigned columns=fb_mode==FB_DESTINATION?3:2;int w=(fbp_w()-64-8*(int)(columns-1))/(int)columns;unsigned at=(unsigned)(x-32)/(unsigned)(w+8);
  if(at>=columns||(x-32)%(w+8)>=w)return false;
  if(at==0){if(fb_mode==FB_DESTINATION)fbx_cancel();else return fb_back();}
  else if(fb_mode==FB_DESTINATION){if(at==1){if(fb_parent(fb_path))(void)fb_load(0);}else fbx_commit_destination();}
  else if(fbs_confirm_ready()&&fbs_choose(fbs_selected))fb_activate();
  return false;
 }
 if(!fbs_hit_has_row)return false;
 unsigned row=(unsigned)fbs_hit_row;
 if(!fbs_choose(row))return false;
 if(fbs_directory_mode()){
  const risc_storage_dirent_v1 *current=&fb_rows[fb_choice];
  char path[FB_PATH_CAP];uint64_t size=0;bool directory=false;
  if(!fb_ready()||!fb_join(fb_path,current->name,path,sizeof(path))||
     !fb_volume->stat(fb_volume->context,path,&size,&directory)||
     directory!=(current->is_directory!=0)||(!directory&&size!=current->size)){
   fb_message("Item changed. Choose it again.");return false;
  }
  if(strcmp(current->name,fbs_hit_item.row.name)||current->is_directory!=fbs_hit_item.row.is_directory||current->size!=fbs_hit_item.row.size){fb_message("Folder changed. Choose the item again.");return false;}
 }else if(fb_mode==FB_HANDLERS){
  t5_file_handler_t current={0};
  if(!portable_file_browser_safe()||!fb_handler_api->handler_get(fb_handler_path,row,&current)||
     memcmp(&current,&fbs_hit_item.handler,sizeof(current))){fb_message("Handlers changed. Choose again.");return false;}
 }
 if(fb_mode==FB_LIST&&fb_manage&&x>=fbp_w()-96)fbx_actions();else fb_activate();
 fb_dirty=true;return false;
}
static void fbs_input(t5_app_input_t *input) {
 fbs_prepare();fbs_hit_has_row=false;
 portable_touch_sample sample;uint32_t now;portable_file_browser_touch_sample(&sample,&now);
 if(input->buttons||input->exit_requested){portable_file_browser_scroll_interrupt();return;}
 input->tapped=false;
 static int hit_q8;static unsigned hit_origin;
 if(sample.began){
  hit_q8=fbs_scroll.position_q8;hit_origin=fbs_origin;
  fbs_hit_generation=fbs_generation;fbs_hit_ready=true;
  int row=portable_scroll_row(&fbs_scroll,sample.x,sample.y);
  /* Snapshot the current logical row at DOWN, independently of paint. A
   * refill or inventory change must still invalidate the original identity. */
  if(row>=0){
   unsigned absolute=hit_origin+(unsigned)row;
   if(fbs_directory_mode()){
    if(fbs_cache_for(absolute)&&fbs_generation==fbs_hit_generation&&
       absolute>=fbs_cache_start&&absolute-fbs_cache_start<fb_count)
     fbs_hit_item.row=fb_rows[absolute-fbs_cache_start];
    else fbs_hit_ready=false;
   }else if(fb_mode==FB_HANDLERS){
    if(fbs_handlers_for(absolute)&&absolute>=fb_handler_offset&&
       absolute-fb_handler_offset<FB_MAX_PAGE_ROWS&&absolute<fb_handler_count)
     fbs_hit_item.handler=fb_handlers[absolute-fb_handler_offset];
    else fbs_hit_ready=false;
   }
  }
 }
 unsigned change=portable_scroll_touch(&fbs_scroll,&sample,now);
 if(change&PORTABLE_SCROLL_BACK){input->buttons=T5_APP_BUTTON_BACK;portable_file_browser_scroll_interrupt();return;}
 if((change&PORTABLE_SCROLL_TAP)&&fbs_hit_ready&&fbs_hit_generation==fbs_generation){portable_touch_scroll visible=fbs_scroll;visible.position_q8=hit_q8;int row=portable_scroll_row(&visible,sample.x,sample.y);fbs_hit_has_row=row>=0;if(row>=0)fbs_hit_row=hit_origin+(unsigned)row;input->tapped=true;input->touch_x=(int16_t)sample.x;input->touch_y=(int16_t)sample.y;}
 if(change&PORTABLE_SCROLL_CHANGED){fbs_slide_coordinates();fb_dirty=true;}
}

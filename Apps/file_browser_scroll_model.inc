/* A moving, naturally sorted window. Every provider directory is closed before
 * input/rendering; memory is independent of the number of files in a folder. */
#define FBS_TOP 112
#define FBS_ROW 88
#define FBS_COORDINATE_ROWS 4096u
static portable_touch_scroll fbs_scroll;
static unsigned fbs_origin;
static unsigned fbs_generation,fbs_mode,fbs_cache_start,fbs_selected,fbs_directory_selected;
static bool fbs_hit_ready,fbs_inventory_valid;
static uint32_t fbs_inventory_sum,fbs_inventory_xor,fbs_inventory_count;
static void fbs_prepare(void);
static void fbs_visible(void);
static bool fbs_move(int direction);
static void fbs_reveal(unsigned row);
static void fbs_reset(void) {
 memset(&fbs_scroll,0,sizeof(fbs_scroll));fbs_generation=1;fbs_mode=UINT32_MAX;
 fbs_origin=fbs_cache_start=fbs_selected=fbs_directory_selected=0;fbs_hit_ready=fbs_inventory_valid=false;
}
void portable_file_browser_scroll_interrupt(void) {
 portable_scroll_cancel(&fbs_scroll);fbs_hit_ready=false;
}
static bool fbs_directory_mode(void){return fb_mode==FB_LIST||fb_mode==FB_DESTINATION;}
static bool fbs_list_mode(void){return fbs_directory_mode()||fb_mode==FB_OPTIONS||fb_mode==FB_ACTIONS||fb_mode==FB_HANDLERS;}
static bool fbs_read(int direction,const risc_storage_dirent_v1 *cursor,unsigned anchor) {
 fb_count=0;fb_matched=0;fb_more=false;fb_loaded=false;
 if(!fb_ready()){portable_file_browser_scroll_interrupt();++fbs_generation;return false;}
 risc_storage_dir_t directory=fb_volume->dir_open(fb_volume->context,fb_path);
 if(!directory){portable_file_browser_scroll_interrupt();++fbs_generation;fb_error("Cannot open folder. Back or Refresh.");return false;}
 risc_storage_dirent_v1 item;unsigned examined=0;bool invalid=false;uint32_t sum=0,xor_hash=0;
 while(fb_volume->dir_next(fb_volume->context,directory,&item)) {
  if((++examined&63u)==0)fb_runtime->yield_ms(1);
  if(!fb_name_valid(item.name,sizeof(item.name))||item.is_directory>1){invalid=true;break;}
  if(fb_mode==FB_DESTINATION&&!item.is_directory)continue;
  if((!fb_hidden&&item.name[0]=='.')||!fb_contains(item.name,fb_query))continue;
  if(fb_matched<UINT32_MAX)++fb_matched;
  uint32_t hash=2166136261u;
  for(unsigned i=0;item.name[i];++i)hash=(hash^(uint8_t)item.name[i])*16777619u;
  hash=(hash^item.is_directory)*16777619u;
  for(unsigned i=0;i<8;++i)hash=(hash^(uint8_t)(item.size>>(i*8)))*16777619u;
  sum+=hash;xor_hash^=hash*2654435761u;
  int order=cursor?fb_compare(&item,cursor):1;
  if(cursor&&(direction>0?order<=0:order>=0))continue;
  unsigned pos=0;
  while(pos<fb_count&&(direction<0?fb_compare(&fb_rows[pos],&item)>0:fb_compare(&fb_rows[pos],&item)<0))++pos;
  if(pos<FB_MAX_PAGE_ROWS){
   if(fb_count<FB_MAX_PAGE_ROWS)++fb_count;
   for(unsigned j=fb_count-1;j>pos;--j)fb_rows[j]=fb_rows[j-1];
   fb_rows[pos]=item;
  }
 }
 const risc_storage_volume_api_v1_ext *ext=risc_storage_volume_extension(fb_volume);
 char error[96]={0};bool failed=ext&&ext->handle_error?ext->handle_error(fb_volume->context,directory,true)!=0:fb_volume->last_error(fb_volume->context,error,sizeof(error))&&error[0];
 if(!fbx_close_dir(fb_volume,directory))failed=true;
 if(invalid||failed){fb_count=0;portable_file_browser_scroll_interrupt();++fbs_generation;fb_message(invalid?"Invalid directory data. Try Refresh.":"Folder read failed. Try Refresh.");return false;}
 bool changed=fbs_inventory_valid&&(fbs_inventory_count!=fb_matched||fbs_inventory_sum!=sum||fbs_inventory_xor!=xor_hash);
 fbs_inventory_valid=true;fbs_inventory_count=fb_matched;fbs_inventory_sum=sum;fbs_inventory_xor=xor_hash;
 if(changed&&cursor){
  portable_file_browser_scroll_interrupt();++fbs_generation;fbs_origin=fbs_cache_start=fbs_selected=fbs_directory_selected=0;fbs_scroll.position_q8=0;
  bool ok=fbs_read(1,NULL,0);if(ok)fb_message("Folder changed. Choose the item again.");return ok;
 }
 if(direction<0)for(unsigned i=0;i<fb_count/2;i++){item=fb_rows[i];fb_rows[i]=fb_rows[fb_count-1-i];fb_rows[fb_count-1-i]=item;}
 fbs_cache_start=direction<0?(anchor>fb_count?anchor-fb_count:0):anchor;
 fb_page=0;fb_more=fbs_cache_start+fb_count<fb_matched;fb_loaded=true;
 fb_choice=fbs_selected>=fbs_cache_start&&fbs_selected<fbs_cache_start+fb_count?fbs_selected-fbs_cache_start:UINT32_MAX;
 fb_manage=fb_volume->remove||fb_volume->file_open_write||(ext&&ext->rename);
 return true;
}
static bool fbs_cache_for(unsigned first) {
 if(!fb_loaded)return false;
 if(!fb_matched)return true;
 unsigned generation=fbs_generation;
 unsigned rows=(unsigned)fbs_scroll.view.height/FBS_ROW+2;
 if(rows>FB_MAX_PAGE_ROWS)rows=FB_MAX_PAGE_ROWS;
 if(first>=fb_matched)first=fb_matched-1;
 unsigned end=rows>fb_matched-first?fb_matched:first+rows;
 while(first<fbs_cache_start || end>fbs_cache_start+fb_count) {
  if(!fb_count)return false;
  unsigned anchor;int direction; risc_storage_dirent_v1 cursor;
  if(first<fbs_cache_start){
   unsigned after=rows>fb_matched-first?fb_matched:first+rows;
   unsigned index=after>fbs_cache_start?after-fbs_cache_start:0;
   if(index>=fb_count)index=fb_count-1;
   cursor=fb_rows[index];anchor=fbs_cache_start+index;direction=-1;
  }else {
   unsigned index=first>fbs_cache_start?first-fbs_cache_start-1:0;
   if(index>=fb_count)index=fb_count-1;
   cursor=fb_rows[index];anchor=fbs_cache_start+index+1;direction=1;
  }
  unsigned old=fbs_cache_start,old_count=fb_count;
  if(!fbs_read(direction,&cursor,anchor)||generation!=fbs_generation)return false;
  if(fbs_cache_start==old&&fb_count==old_count)return false;
  if(end>fb_matched)end=fb_matched;
 }
 return true;
}
static bool fbs_load(int direction) {
 if(direction&&fb_loaded){
  unsigned step=(unsigned)fbs_scroll.view.height/FBS_ROW,target;
  if(direction<0)target=fbs_selected>step?fbs_selected-step:0;
  else target=fb_matched&&step<fb_matched-fbs_selected?fbs_selected+step:fb_matched?fb_matched-1:0;
  fbs_selected=target;fbs_reveal(target);
  bool ok=fbs_cache_for(fbs_origin+(unsigned)portable_scroll_offset(&fbs_scroll)/FBS_ROW);fb_dirty=true;return ok;
 }
 portable_file_browser_scroll_interrupt();++fbs_generation;fbs_inventory_valid=false;fbs_origin=fbs_cache_start=fbs_selected=fbs_directory_selected=0;fbs_scroll.position_q8=0;
 bool ok=fbs_read(1,NULL,0);
 if(ok){fb_message(fb_mode==FB_DESTINATION?"Choose folder; HERE confirms":!fb_count?(fb_query[0]?"No matching files":"This folder is empty"):fb_manage?"Select item or open options":"Browse files / read-only volume");fb_feedback=false;}
 return ok;
}

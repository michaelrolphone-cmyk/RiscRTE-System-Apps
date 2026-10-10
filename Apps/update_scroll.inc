/* Selected paper client only. Provider indices never identify an action by
 * themselves: completed pixels and the exact fresh release travel together. */
#if !defined(PORTABLE_TOUCH_SCROLL) || !defined(PORTABLE_APP_TOUCH_SCROLL) || !defined(PORTABLE_NATIVE_CUSTODY_FENCE) || !defined(PORTABLE_NATIVE_TIME_TOOLBAR)
#error "Update touch scrolling requires explicit native paper scrolling"
#endif
#include "PortablePaperScroll.h"
#define UPS_ROWS 6u
#define UPS_TOP 108
#define UPS_EXTENT 96
#define UPS_HEIGHT 384
enum {UPS_CATALOG,UPS_CONFIRM,UPS_PROGRESS,UPS_CLEANUP,UPS_RESTART};
typedef struct {
 unsigned generation,mode,selected,first,count,total;
 int q8;
 software_update_row_v1 rows[UPS_ROWS],release;
 bool valid[UPS_ROWS];
} ups_image;
static portable_touch_scroll ups_scroll;
static ups_image ups_submitted,ups_completed;
static unsigned ups_generation,ups_mode,ups_total,ups_hit_generation,ups_hit_mode,ups_hit_selected;
static uint32_t ups_inventory;
static bool ups_inventory_valid,ups_hit_valid;
static int ups_hit_row,ups_hit_footer;
static software_update_row_v1 ups_hit_release;
static unsigned ups_current_mode(void) {
 return restart_pending?UPS_RESTART:cleanup_pending?UPS_CLEANUP:
  connecting||ust.state==SOFTWARE_UPDATE_CATALOG||(ust.state>=SOFTWARE_UPDATE_PREPARING&&ust.state<=SOFTWARE_UPDATE_READY)?UPS_PROGRESS:
  confirming?UPS_CONFIRM:UPS_CATALOG;
}
static bool ups_string(const char *text,unsigned capacity) {
 for(unsigned i=0;i<capacity;i++)if(!text[i])return true;return false;
}
static bool ups_get(unsigned row,software_update_row_v1 *out) {
 *out=(software_update_row_v1){.struct_size=sizeof(*out)};
 return !PORTABLE_APP_RETAINED()&&row&&row<=ust.count&&row<=SOFTWARE_UPDATE_ROWS_MAX&&us&&us->get(us->context,row-1,out)&&!PORTABLE_APP_RETAINED()&&
  ups_string(out->id,sizeof(out->id))&&ups_string(out->version,sizeof(out->version))&&
  ups_string(out->installed,sizeof(out->installed))&&ups_string(out->reason,sizeof(out->reason));
}
static bool ups_same(const software_update_row_v1 *a,const software_update_row_v1 *b) {
 return a->availability==b->availability&&a->size==b->size&&!strcmp(a->id,b->id)&&
  !strcmp(a->version,b->version)&&!strcmp(a->installed,b->installed)&&!strcmp(a->reason,b->reason);
}
static void ups_interrupt(void) {portable_scroll_cancel(&ups_scroll);ups_hit_valid=false;}
static void ups_reset(void) {
 memset(&ups_scroll,0,sizeof(ups_scroll));memset(&ups_submitted,0,sizeof(ups_submitted));memset(&ups_completed,0,sizeof(ups_completed));
 ups_generation=1;ups_mode=~0u;ups_inventory_valid=ups_hit_valid=false;
}
static uint32_t ups_hash(uint32_t hash,const char *text) {
 do {hash=(hash^(uint8_t)*text)*16777619u;}while(*text++);return hash;
}
static bool ups_status(void) {
 if(PORTABLE_APP_RETAINED())return false;
 software_update_status_v1 next={.struct_size=sizeof(next)};
 if(!us||!us->status(us->context,&next)){cleanup_pending=true;message="Service status unavailable";dirty=true;return false;}
 if(memcmp(&ust,&next,sizeof(ust)))dirty=true;ust=next;
 if(ust.state==SOFTWARE_UPDATE_RETAINED){PORTABLE_APP_RETAIN();cleanup_pending=true;return false;}return true;
}
static void ups_prepare(void) {
 if(PORTABLE_APP_RETAINED())return;
 if(us&&!cleanup_pending&&!restart_pending)(void)ups_status();
 if(PORTABLE_APP_RETAINED())return;
 unsigned mode=ups_current_mode();
 unsigned total=mode==UPS_CATALOG?(ust.count<=SOFTWARE_UPDATE_ROWS_MAX?ust.count+1:1):mode==UPS_CONFIRM?2:1;
 bool changed=mode!=ups_mode;
 if(mode==UPS_CATALOG) {
  uint32_t hash=2166136261u;software_update_row_v1 row;
  for(unsigned i=1;i<total;++i){
   bool valid=ups_get(i,&row);hash=(hash^(valid?i:~i))*16777619u;
   if(valid){hash=ups_hash(hash,row.id);hash=ups_hash(hash,row.version);hash=ups_hash(hash,row.installed);hash=ups_hash(hash,row.reason);hash=(hash^row.availability)*16777619u;hash=(hash^row.size)*16777619u;}
  }
  if(ups_inventory_valid&&(hash!=ups_inventory||total!=ups_total)){
   changed=true;selected=0;message="Catalog changed; choose the release again";
  }
  ups_inventory=hash;ups_inventory_valid=true;
 }else ups_inventory_valid=false;
 if(changed){ups_interrupt();++ups_generation;ups_scroll.position_q8=0;dirty=true;}
 ups_mode=mode;ups_total=total;
 if(mode==UPS_CATALOG&&selected>=total)selected=0;
 portable_scroll_configure(&ups_scroll,(portable_scroll_viewport){32,UPS_TOP,ua->screen_width()-64,UPS_HEIGHT},UPS_EXTENT,(int)total);
 if(changed)portable_scroll_reveal(&ups_scroll,(int)(mode==UPS_CONFIRM?confirm_choice:mode==UPS_CATALOG?selected:0));
}
static void ups_visible(void) {
 if(portable_paper_scroll_settled())ups_completed=ups_submitted;
}
static bool ups_selected_current(void) {
 software_update_row_v1 current;
 if(ups_status()&&ups_get(selected,&current)&&current.availability==SOFTWARE_UPDATE_AVAILABLE&&ups_same(&current,&selected_row))return true;
 confirming=false;install_after_connect=false;ups_interrupt();++ups_generation;
 message="Release changed; choose and confirm again";dirty=true;return false;
}
static bool ups_confirm_ready(void) {
 ups_prepare();if(PORTABLE_APP_RETAINED())return false;ups_visible();
 unsigned choice=ups_mode==UPS_CONFIRM?confirm_choice:ups_mode==UPS_CATALOG?selected:0;
 portable_touch_scroll shown=ups_scroll;shown.position_q8=ups_completed.q8;
 int y=portable_scroll_row_y(&shown,(int)choice);
 if(ups_completed.generation!=ups_generation||ups_completed.mode!=ups_mode||ups_completed.selected!=choice||
    y<shown.view.y||y+UPS_EXTENT>shown.view.y+shown.view.height){
  portable_scroll_reveal(&ups_scroll,(int)choice);dirty=true;return false;
 }
 if(ups_mode==UPS_CONFIRM)return ups_same(&ups_completed.release,&selected_row)&&ups_selected_current();
 if(ups_mode==UPS_CATALOG&&choice){
  unsigned at=choice-ups_completed.first;software_update_row_v1 current;
  if(at>=ups_completed.count||!ups_completed.valid[at]||!ups_get(choice,&current)||!ups_same(&current,&ups_completed.rows[at])){
   message="Release changed; choose it again";dirty=true;return false;
  }
 }
 return true;
}
static void ups_render(void) {
 ups_prepare();if(PORTABLE_APP_RETAINED()||!paper_frame_ready()||!portable_paper_scroll_available())return;ups_visible();
 int w=ua->screen_width(),h=ua->screen_height();unsigned first=(unsigned)portable_scroll_offset(&ups_scroll)/UPS_EXTENT;
 ups_image image={.generation=ups_generation,.mode=ups_mode,.selected=ups_mode==UPS_CONFIRM?confirm_choice:ups_mode==UPS_CATALOG?selected:0,
  .first=first,.total=ups_total,.q8=ups_scroll.position_q8,.release=selected_row};
 /* Fetch before acquiring display custody. Keep only the visible identities. */
 image.count=ups_total-first;if(image.count>UPS_ROWS)image.count=UPS_ROWS;
 if(ups_mode==UPS_CATALOG)for(unsigned i=0;i<image.count;i++)if(first+i)image.valid[i]=ups_get(first+i,&image.rows[i]);
 if(PORTABLE_APP_RETAINED())return;
 up->begin();up_text(32,28,w-64,PORTABLE_UPDATE_FIRMWARE?"FIRMWARE UPDATE":"APP STORE",true,true);ua->fill_rect(32,88,w-64,3,true);
 portable_paper_scroll_clip(&ups_scroll.view);
 for(unsigned i=0;i<image.count;i++) {
  unsigned row=first+i;int y=portable_scroll_row_y(&ups_scroll,(int)row);bool chosen=row==image.selected;char value[96];
  const char *name="",*detail="";
  if(ups_mode==UPS_CATALOG){
   if(!row){name="Check / retry";detail="Repository release catalog";}
   else if(!image.valid[i]){name="Unavailable";detail="Retry Check";}
   else {const software_update_row_v1 *r=&image.rows[i];name=r->id;
    const char *state=r->availability==SOFTWARE_UPDATE_AVAILABLE?"Update":r->availability==SOFTWARE_UPDATE_CURRENT?"Current":r->availability==SOFTWARE_UPDATE_USB_ONLY?"USB only":"Unsupported";
    snprintf(value,sizeof(value),"%s  %s",r->version,state);detail=value;}
  }else if(ups_mode==UPS_CONFIRM){name=row?"Install and restart":"Cancel";detail=row?selected_row.version:"Keep current installation";}
  else if(ups_mode==UPS_RESTART){name="Restart device";detail=ust.state==SOFTWARE_UPDATE_ACTIVATION_UNKNOWN?"Activation uncertain; restart required":"Activation confirmed; restart required";}
  else if(ups_mode==UPS_CLEANUP){name="Retry cleanup";detail="Resources retained until cleanup succeeds";}
  else {name="Cancel operation";snprintf(value,sizeof(value),"%lu / %lu bytes",(unsigned long)ust.done,(unsigned long)ust.total);detail=value;}
  if(chosen)ua->fill_rect(32,y,w-64,UPS_EXTENT,true);
  up_text(48,y+12,w-128,name,false,!chosen);up_text(48,y+46,w-128,detail,false,!chosen);
  if(ua->draw_icon)ua->draw_icon(w-72,y+34,"solid:f054",24,!chosen);ua->fill_rect(32,y+93,w-64,3,true);
 }
 portable_paper_scroll_clip(NULL);
 if(ups_scroll.limit){int bar=UPS_HEIGHT*UPS_HEIGHT/(int)(ups_total*UPS_EXTENT);if(bar<24)bar=24;
  ua->fill_rect(w-24,UPS_TOP+(UPS_HEIGHT-bar)*portable_scroll_offset(&ups_scroll)/ups_scroll.limit,4,bar,true);}
 const char *detail=ups_mode==UPS_CONFIRM?selected_row.id:ups_mode==UPS_PROGRESS?(connecting?"Connecting...":ust.state==SOFTWARE_UPDATE_CATALOG?"Checking catalog":ust.state==SOFTWARE_UPDATE_PREPARING?"Preparing update":ust.state==SOFTWARE_UPDATE_DOWNLOAD?"Downloading update":"Verifying update"):"";
 up_text(32,512,w-64,detail,false,true);
 const char *s=message?message:"";unsigned pos=0;
 for(unsigned row=0;row<4&&s[pos];row++){char line[97]={0};unsigned n=0;while(n<96&&s[pos+n]){line[n]=s[pos+n];line[n+1]=0;if(up->measure(line,false)>w-64){line[n]=0;break;}n++;}if(!n)break;up_text(32,548+(int)row*28,w-64,line,false,true);pos+=n;}
 const char *labels[]={"BACK","PREV",up_action(),"NEXT"};const char *icons[]={"solid:f053","solid:f077",NULL,"solid:f078"};int bw=(w-88)/4;
 for(unsigned i=0;i<4;i++){int x=32+(int)i*(bw+8);ua->fill_rect(x,h-112,bw,88,true);ua->fill_rect(x+3,h-109,bw-6,82,false);
  if(icons[i]&&ua->draw_icon)ua->draw_icon(x+(bw-24)/2,h-98,icons[i],24,true);up_text(x+5,h-62,bw-10,labels[i],false,true);}
 ua->present(ups_submitted.generation!=image.generation);ups_submitted=image;ups_visible();dirty=false;
}
static int ups_footer(int x,int y) {
 int w=ua->screen_width(),h=ua->screen_height(),bw=(w-88)/4;x-=32;
 if(x<0||y<h-112||y>=h-24)return -1;
 int at=x/(bw+8);return at<4&&x%(bw+8)<bw?at:-1;
}
static void ups_input(t5_app_input_t *input) {
 ups_prepare();input->tapped=false;if(PORTABLE_APP_RETAINED())return;ups_visible();
 if(input->buttons||input->exit_requested||!portable_paper_scroll_available()){ups_interrupt();return;}
 portable_touch_sample sample;uint32_t now;portable_paper_scroll_sample(&sample,&now);
 if(sample.began){
  ups_hit_footer=ups_footer(sample.x,sample.y);
  ups_hit_generation=ups_completed.generation;ups_hit_mode=ups_completed.mode;ups_hit_selected=ups_completed.selected;
  ups_hit_valid=ups_hit_generation==ups_generation&&ups_hit_mode==ups_mode;
  portable_touch_scroll shown=ups_scroll;shown.position_q8=ups_completed.q8;ups_hit_row=portable_scroll_row(&shown,sample.x,sample.y);
  if(ups_mode==UPS_CATALOG&&ups_hit_row>0){unsigned at=(unsigned)ups_hit_row-ups_completed.first;
   if(at>=ups_completed.count||!ups_completed.valid[at])ups_hit_valid=false;else ups_hit_release=ups_completed.rows[at];}
 }
 unsigned result=portable_scroll_touch(&ups_scroll,&sample,now);
 if(result&PORTABLE_SCROLL_CHANGED)dirty=true;
 if(result&PORTABLE_SCROLL_BACK){input->buttons=T5_APP_BUTTON_BACK;ups_interrupt();return;}
 if(!(result&PORTABLE_SCROLL_TAP))return;
 int footer=ups_footer(sample.x,sample.y);
 /* A fresh Cancel or Back does not wait for the progress image to finish. */
 if(footer>=0&&footer==ups_hit_footer){
  if(footer==0){input->buttons=T5_APP_BUTTON_BACK;return;}
  if(footer==2&&(ups_mode==UPS_PROGRESS||(ups_mode==UPS_CONFIRM&&!confirm_choice))){input->buttons=T5_APP_BUTTON_CONFIRM;return;}
 }
 if(!ups_hit_valid||ups_hit_generation!=ups_generation||ups_hit_mode!=ups_mode)return;
 int w=ua->screen_width(),h=ua->screen_height(),bw=(w-88)/4,x=(int)sample.x-32;
 if(x>=0&&sample.y>=h-112&&sample.y<h-24){unsigned at=(unsigned)x/(unsigned)(bw+8);
  if(at<4&&x%(bw+8)<bw){static const unsigned keys[]={T5_APP_BUTTON_BACK,T5_APP_BUTTON_UP,T5_APP_BUTTON_CONFIRM,T5_APP_BUTTON_DOWN};
   unsigned current=ups_mode==UPS_CONFIRM?confirm_choice:ups_mode==UPS_CATALOG?selected:0;
   if(at!=2||ups_hit_selected==current)input->buttons=keys[at];}return;
 }
 if(ups_hit_row<0||(unsigned)ups_hit_row>=ups_total)return;
 if(ups_mode==UPS_CATALOG){software_update_row_v1 current;
  if(ups_hit_row&&(!ups_get((unsigned)ups_hit_row,&current)||!ups_same(&current,&ups_hit_release))){message="Release changed; choose it again";dirty=true;return;}
  selected=(unsigned)ups_hit_row;
 }else if(ups_mode==UPS_CONFIRM)confirm_choice=(unsigned)ups_hit_row;
 dirty=true;
}

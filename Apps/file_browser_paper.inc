/* Static portrait e-paper presentation for the same volume/controller model. */
#include "PaperFrame.h"
#define FBP_TOP 112
#define FBP_ROW 88
static unsigned fbp_last_mode;
static bool fbp_clean,fbp_contact_down;
static int fbp_start_x,fbp_start_y,fbp_last_x,fbp_last_y;
static int fbp_w(void){return fb_app->screen_width();}
static int fbp_h(void){return fb_app->screen_height();}
static void fbp_text(int x,int y,int w,const char *text,bool heading,bool black,bool literal){fb_paper->text(x,y,w,text,(heading?2u:1u)|(literal?PAPER_TEXT_LITERAL:0u),heading,black);}
static void fbp_icon(int x,int y,const char *name,unsigned size,bool black){if(fb_app->draw_icon)(void)fb_app->draw_icon(x,y,name,(uint8_t)size,black);}
static void fbp_button(int x,int y,int w,int h,const char *label,const char *glyph,bool selected) {
 fb_app->fill_rect(x,y,w,h,true);if(!selected)fb_app->fill_rect(x+3,y+3,w-6,h-6,false);
 if(glyph)fbp_icon(x+(w-24)/2,y+12,glyph,24,!selected);
 int n=fb_paper->measure(label,false);fbp_text(x+(w-n)/2,y+(glyph?48:(h-26)/2),w-10,label,false,!selected,false);
}
static void fbp_wrap(int x,int y,int width,unsigned lines,const char *text,bool black) {
 unsigned at=0;for(unsigned row=0;row<lines&&text[at];row++) {
  char line[129]={0};unsigned n=0;
  while(n<128&&text[at+n]){line[n]=text[at+n];line[n+1]=0;if(fb_paper->measure(line,false)>width){line[n]=0;break;}n++;}
  if(!n)break;
  fbp_text(x,y+(int)row*28,width,line,false,black,true);at+=n;
 }
}
static unsigned fbp_keys(void){unsigned rows=(unsigned)(fbp_h()-448)/88;return (rows?rows:1)*5;}
static void fbp_keyboard(void) {
 fbp_text(32,28,fbp_w()-64,fb_mode==FB_RENAME?"RENAME":"FIND FILE",true,true,false);
 fb_app->fill_rect(32,100,fbp_w()-64,3,true);fbp_wrap(32,120,fbp_w()-64,2,fb_editor,true);
 fbp_text(32,178,fbp_w()-64,fb_status,false,true,false);
 unsigned keys=fbp_keys();int width=(fbp_w()-64)/5;
 for(unsigned i=0;i<keys;i++){unsigned c=32+fbp_key_page*keys+i;if(c>126)break;char s[2]={(char)c,0};int x=32+(int)(i%5)*width,y=208+(int)(i/5)*88;
  fbp_button(x,y,width-3,88,"",NULL,false);fbp_text(x+width/2-7,y+30,width-14,c==32?"_":s,false,true,true);}
 int half=(fbp_w()-80)/2;
 fbp_button(32,fbp_h()-216,half,88,"NEXT KEYS",NULL,false);fbp_button(fbp_w()/2+8,fbp_h()-216,half,88,"DELETE",NULL,false);
 fbp_button(32,fbp_h()-112,half,88,"CANCEL",NULL,false);fbp_button(fbp_w()/2+8,fbp_h()-112,half,88,"DONE",NULL,true);
}
static void fbp_footer(void) {
 const char *names[]={"BACK","PREV",fb_mode==FB_PREVIEW?(fb_hex?"TEXT":"HEX"):fb_mode==FB_DETAIL?"READ":"OPEN","NEXT"};
 const char *icons[]={"solid:f053","solid:f053",NULL,"solid:f054"};int width=(fbp_w()-88)/4;
 for(unsigned i=0;i<4;i++)fbp_button(32+(int)i*(width+8),fbp_h()-112,width,88,names[i],icons[i],i==2);
}
static void fbp_management(void) {
 fbp_text(32,28,fbp_w()-152,fb_mode==FB_DESTINATION?(fb_moving?"MOVE TO":"COPY TO"):fb_mode==FB_DELETE?"DELETE ITEM":fb_mode==FB_HANDLERS?"OPEN WITH":fb_mode==FB_NOTICE?"FILE STATUS":"FILE ACTIONS",true,true,false);
 fbp_text(32,72,fbp_w()-64,fb_mode==FB_DESTINATION?fb_path:fb_selected.name,false,true,true);fb_app->fill_rect(32,106,fbp_w()-64,3,true);
 if(fb_mode==FB_ACTIONS||fb_mode==FB_HANDLERS){
  unsigned start=fb_mode==FB_ACTIONS?fb_action_page*fb_page_rows:0,total=fb_mode==FB_ACTIONS?FBX_ACTION_COUNT:fb_handler_count-fb_handler_offset;
  for(unsigned row=0;row<fb_page_rows&&start+row<total;row++){unsigned at=start+row;int y=FBP_TOP+(int)row*FBP_ROW;bool chosen=at==fb_choice;
   if(chosen)fb_app->fill_rect(32,y,fbp_w()-64,FBP_ROW,true);
   fbp_text(48,y+22,fbp_w()-128,fb_mode==FB_ACTIONS?fbx_labels[at]:fb_handlers[at].display_name,false,!chosen,fb_mode==FB_HANDLERS);
   fbp_icon(fbp_w()-72,y+30,"solid:f054",24,!chosen);fb_app->fill_rect(32,y+FBP_ROW-3,fbp_w()-64,3,true);
  }
  if(fb_mode==FB_ACTIONS){char label[64];unsigned end=start+fb_page_rows;if(end>FBX_ACTION_COUNT)end=FBX_ACTION_COUNT;snprintf(label,sizeof(label),"%u-%u OF %u ACTIONS",start+1,end,FBX_ACTION_COUNT);fbp_text(32,fbp_h()-146,fbp_w()-64,label,false,true,false);}
  if(fb_mode==FB_HANDLERS){char label[64];snprintf(label,sizeof(label),"%u-%u OF %u HANDLERS",fb_handler_offset+1,fb_handler_offset+(total<fb_page_rows?total:fb_page_rows),fb_handler_count);fbp_text(32,fbp_h()-146,fbp_w()-64,label,false,true,false);}
  fbp_footer();
 }else if(fb_mode==FB_DESTINATION){
  if(!fb_moving)fbp_button(fbp_w()-120,8,88,88,"VOL",NULL,false);
  for(unsigned row=0;row<fb_count;row++){int y=FBP_TOP+(int)row*FBP_ROW;fbp_icon(48,y+26,"solid:f07c",32,true);fbp_text(96,y+25,fbp_w()-160,fb_rows[row].name,false,true,true);fb_app->fill_rect(32,y+FBP_ROW-3,fbp_w()-64,3,true);}
  fbp_text(32,fbp_h()-146,fbp_w()-64,fb_status,false,true,false);
  const char *labels[]={"CANCEL","UP","HERE","NEXT"};int w=(fbp_w()-88)/4;
  for(unsigned i=0;i<4;i++)fbp_button(32+(int)i*(w+8),fbp_h()-112,w,88,labels[i],NULL,i==2);
 }else if(fb_mode==FB_DELETE){
  fbp_wrap(32,144,fbp_w()-64,4,fb_selected.is_directory?"Permanently delete this folder and all of its contents?":"Permanently delete this file?",true);
  fbp_wrap(32,320,fbp_w()-64,5,fb_selected.name,true);fbp_wrap(32,512,fbp_w()-64,3,"This cannot be undone.",true);
  int half=(fbp_w()-80)/2;fbp_button(32,fbp_h()-112,half,88,"CANCEL",NULL,fb_choice==0);fbp_button(fbp_w()/2+8,fbp_h()-112,half,88,"DELETE",NULL,fb_choice==1);
 }else {fbp_wrap(32,144,fbp_w()-64,12,fb_status,true);fbp_button(32,fbp_h()-112,fbp_w()-64,88,"BACK",NULL,false);}
}
static void fbp_draw(void) {
 if(!paper_frame_ready())return;
 fb_paper->begin();
 if(fb_mode==FB_FILTER||fb_mode==FB_RENAME)fbp_keyboard();
 else if(fb_mode>=FB_ACTIONS)fbp_management();
 else {
  fbp_text(32,28,fbp_w()-152,fb_mode==FB_PREVIEW?"PREVIEW":fb_mode==FB_OPTIONS?"FILE OPTIONS":"FILES",true,true,false);
  fbp_text(32,72,fbp_w()-160,fb_path,false,true,true);fb_app->fill_rect(32,106,fbp_w()-64,3,true);
  if(fb_mode==FB_LIST) {
   fbp_button(fbp_w()-120,8,88,88,"","solid:f141",false);
   for(unsigned i=0;i<fb_count;i++) {
    int y=FBP_TOP+(int)i*FBP_ROW;bool chosen=i==fb_choice;char size[32];fb_size_text(fb_rows[i].size,size,sizeof(size));
    if(chosen)fb_app->fill_rect(32,y,fbp_w()-64,FBP_ROW,true);
    fbp_icon(48,y+22,fb_rows[i].is_directory?"solid:f07c":"solid:f15b",32,!chosen);
    fbp_text(96,y+10,fbp_w()-(fb_manage?200:144),fb_rows[i].name,false,!chosen,true);
    if(fb_manage)fbp_icon(fbp_w()-80,y+26,"solid:f141",24,!chosen);
    fbp_text(96,y+42,fbp_w()-144,fb_rows[i].is_directory?"FOLDER":size,false,!chosen,false);
    fb_app->fill_rect(32,y+FBP_ROW-3,fbp_w()-64,3,true);
   }
   if(!fb_count)fbp_wrap(32,150,fbp_w()-64,4,fb_status,true);
   else {char status[64];snprintf(status,sizeof(status),"%u-%u OF %u%s",fb_page*fb_page_rows+1,fb_page*fb_page_rows+fb_count,fb_matched,fb_query[0]?" MATCHES":" ITEMS");fbp_text(32,fbp_h()-146,fbp_w()-64,fb_feedback?fb_status:status,false,true,false);}
  } else if(fb_mode==FB_OPTIONS) {
   for(unsigned i=0;i<fb_option_count();i++){int y=FBP_TOP+(int)i*FBP_ROW;bool chosen=i==fb_choice;if(chosen)fb_app->fill_rect(32,y,fbp_w()-64,FBP_ROW,true);
    fbp_text(48,y+14,fbp_w()-112,fb_options[i],false,!chosen,false);
    if(i==2)fbp_text(48,y+44,fbp_w()-112,fb_hidden?"ON":"OFF",false,!chosen,false);
    fbp_icon(fbp_w()-72,y+30,"solid:f054",24,!chosen);fb_app->fill_rect(32,y+FBP_ROW-3,fbp_w()-64,3,true);}
  } else if(fb_mode==FB_DETAIL) {
   char size[32];fb_size_text(fb_size,size,sizeof(size));fbp_icon(32,128,"solid:f15b",32,true);
   fbp_wrap(80,128,fbp_w()-112,5,fb_selected.name,true);fbp_text(32,300,fbp_w()-64,size,false,true,false);
   fbp_wrap(32,356,fbp_w()-64,4,"Read-only file preview. Files do not change the installed system.",true);
  } else {
   char line[96];snprintf(line,sizeof(line),"%s / BYTE %u",fb_hex?"HEX":"TEXT",(unsigned)fb_offset);fbp_text(32,120,fbp_w()-64,line,false,true,false);
   unsigned pos=0,rows=(unsigned)(fbp_h()-320)/28;
   for(unsigned row=0;row<rows&&pos<fb_preview_count;row++) {
    unsigned n=0;
    if(fb_hex){static const char digits[]="0123456789ABCDEF";for(unsigned col=0;col<8&&pos<fb_preview_count;col++){unsigned b=fb_preview[pos++];line[n++]=digits[b>>4];line[n++]=digits[b&15];line[n++]=' ';}}
    else while(n<32&&pos<fb_preview_count){unsigned c=fb_preview[pos++];line[n++]=(char)(c>=32&&c<=126?c:c=='\n'||c=='\r'||c=='\t'?' ':'.');}
    line[n]=0;fbp_text(32,170+(int)row*28,fbp_w()-64,line,false,true,true);
   }
   if(!fb_preview_count)fbp_wrap(32,180,fbp_w()-64,4,fb_status,true);
  }
  fbp_footer();
 }
 fb_app->present(fbp_clean||fbp_last_mode!=fb_mode);fbp_clean=false;fbp_last_mode=fb_mode;fb_dirty=false;
}
static bool fbp_touch(int x,int y) {
 if(x<32||x>=fbp_w()-32||y<0||y>=fbp_h())return false;
 if(fb_mode==FB_FILTER||fb_mode==FB_RENAME) {
  if(y>=fbp_h()-112&&y<fbp_h()-24){if(x>=fbp_w()/2-8&&x<fbp_w()/2+8)return false;if(x<fbp_w()/2)return fb_back();fb_filter_key(PWK_DONE);return false;}
  if(y>=fbp_h()-216&&y<fbp_h()-128){if(x>=fbp_w()/2-8&&x<fbp_w()/2+8)return false;if(x<fbp_w()/2){fbp_key_page=(fbp_key_page+1)%((95+fbp_keys()-1)/fbp_keys());fb_dirty=true;}else fb_filter_key(PWK_DELETE);return false;}
  unsigned keys=fbp_keys();int width=(fbp_w()-64)/5;
  if(y>=208&&y<208+(int)(keys/5)*88){unsigned col=(unsigned)(x-32)/(unsigned)width,key=(unsigned)(y-208)/88*5+col,c=32+fbp_key_page*keys+key;
   if(col<5&&(x-32)%width<width-3&&key<keys&&c<=126){fb_key_page=(c-32)/32;fb_filter_key((c-32)%32);}}
  return false;
 }
 if(fb_mode==FB_DELETE){if(y>=fbp_h()-112&&y<fbp_h()-24){if(x>=fbp_w()/2-8&&x<fbp_w()/2+8)return false;fb_choice=x<fbp_w()/2?0:1;fbx_activate();fb_dirty=true;}return false;}
 if(fb_mode==FB_NOTICE){if(y>=fbp_h()-112&&y<fbp_h()-24)return fb_back();return false;}
 if(fb_mode==FB_DESTINATION){
  if(!fb_moving&&x>=fbp_w()-120&&y>=8&&y<96){if(fbx_swap()){strcpy(fb_path,"/");(void)fb_load(0);}return false;}
  if(y>=fbp_h()-112&&y<fbp_h()-24){int w=(fbp_w()-88)/4;unsigned at=(unsigned)(x-32)/(unsigned)(w+8);if(at>=4||(x-32)%(w+8)>=w)return false;
   if(at==0)fbx_cancel();else if(at==1){if(fb_parent(fb_path))(void)fb_load(0);}else if(at==2)fbx_commit_destination();else fb_page_next();return false;}
  if(y>=FBP_TOP&&y<FBP_TOP+(int)fb_count*FBP_ROW){fb_choice=(unsigned)(y-FBP_TOP)/FBP_ROW;fbx_activate();}return false;
 }
 if((fb_mode==FB_ACTIONS||fb_mode==FB_HANDLERS)&&y>=FBP_TOP&&y<FBP_TOP+(int)fb_page_rows*FBP_ROW){unsigned at=(unsigned)(y-FBP_TOP)/FBP_ROW+(fb_mode==FB_ACTIONS?fb_action_page*fb_page_rows:0),count=fb_mode==FB_ACTIONS?FBX_ACTION_COUNT:fb_handler_count-fb_handler_offset;if(at<count){fb_choice=at;fbx_activate();fb_dirty=true;}return false;}
 if(fb_mode==FB_LIST&&x>=fbp_w()-120&&y>=8&&y<96){fb_list_choice=fb_choice;fb_mode=FB_OPTIONS;fb_choice=0;fb_dirty=true;return false;}
 if(y>=fbp_h()-112&&y<fbp_h()-24){int width=(fbp_w()-88)/4;unsigned at=(unsigned)(x-32)/(unsigned)(width+8);if(at>=4||(x-32)%(width+8)>=width)return false;
  if(at==0)return fb_back();
  if(fb_mode==FB_ACTIONS&&(at==1||at==3)){if(at==1&&fb_action_page)fb_action_page--;if(at==3&&(fb_action_page+1)*fb_page_rows<FBX_ACTION_COUNT)fb_action_page++;fb_choice=fb_action_page*fb_page_rows;fb_dirty=true;return false;}
  if(at==1)fb_move(-1);else if(at==2)fb_activate();else fb_move(1);return false;}
 if((fb_mode==FB_LIST||fb_mode==FB_OPTIONS)&&y>=FBP_TOP&&y<FBP_TOP+(int)fb_page_rows*FBP_ROW){unsigned at=(unsigned)(y-FBP_TOP)/FBP_ROW;if(at<(fb_mode==FB_OPTIONS?fb_option_count():fb_count)){fb_choice=at;if(fb_mode==FB_LIST&&fb_manage&&x>=fbp_w()-96)fbx_actions();else fb_activate();}}
 return false;
}
static void fbp_input(t5_app_input_t *input) {
 springboard_contact c={0};fb_paper->contact(&c);
 if(!c.valid||c.cancelled){fbp_contact_down=false;return;}
 if(c.began){fbp_contact_down=true;fbp_start_x=fbp_last_x=c.x;fbp_start_y=fbp_last_y=c.y;}
 if(c.down&&fbp_contact_down){fbp_last_x=c.x;fbp_last_y=c.y;}
 if(c.released) {
  int dx=fbp_last_x-fbp_start_x,dy=fbp_last_y-fbp_start_y;
  if(c.tap_eligible){input->tapped=true;input->touch_x=c.x;input->touch_y=c.y;}
  else if(fbp_contact_down&&(fb_mode==FB_LIST||fb_mode==FB_DESTINATION)&&fbp_start_y>=FBP_TOP&&fbp_start_y<FBP_TOP+(int)fb_page_rows*FBP_ROW&&abs(dy)>=44&&abs(dy)>abs(dx)*2){if(dy<0)fb_page_next();else fb_page_previous();fb_dirty=true;}
  fbp_contact_down=false;
 }
}

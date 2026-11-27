/* Native-layout boundary and modeled scene coverage. No hardware acceptance. */
#define PORTABLE_SETTINGS_BOTH_TIMERS
#define main inherited_geometry_main
#include "settings_sleep_defaults_test.c"
#undef main
static void capture(const char*name){
 const char*dir=getenv("SETTINGS_NATIVE_FRAMES");if(!dir)return;
 char path[512];snprintf(path,sizeof(path),"%s/%s.ppm",dir,name);FILE*f=fopen(path,"wb");assert(f);
 fprintf(f,"P6\n%d %d\n255\n",TEST_SETTINGS_RASTER_WIDTH,TEST_SETTINGS_RASTER_HEIGHT);
 for(unsigned i=0;i<sizeof(framebuffer)/sizeof(*framebuffer);++i){unsigned v=framebuffer[i];unsigned char b[]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};assert(fwrite(b,1,3,f)==3);}fclose(f);
}
static void scene(unsigned page,const char*name){sv_page=page;settings_view_redraw();capture(name);}
int main(void){
 begin();int side=sv_native_side();
 for(int n=0;n<240;n++){
  assert(sv_layout_x(sv_native_x(n))==n);assert(sv_layout_y(sv_native_y(n))==n);
  assert(sv_native_x(n+1)>sv_native_x(n));
 }
 int x=SV_SAVE_X,y=SV_ACTION_Y,w=SV_ACTION_W,h=SV_ACTION_H;
 sv_native_button_geometry(&x,&y,&w,&h);
 if(side>240){assert(sv_native_y(y+h)-sv_native_y(y)>=44);assert(sv_native_x(240)-sv_native_x(0)==side);}
 assert(sv_inside(x,y,SV_SAVE_X,SV_ACTION_Y,SV_ACTION_W,SV_ACTION_H));
 assert(sv_inside(x+w-1,y+h-1,SV_SAVE_X,SV_ACTION_Y,SV_ACTION_W,SV_ACTION_H));
 assert(!sv_inside(x-1,y,SV_SAVE_X,SV_ACTION_Y,SV_ACTION_W,SV_ACTION_H));
 assert(!sv_inside(x+w,y,SV_SAVE_X,SV_ACTION_Y,SV_ACTION_W,SV_ACTION_H));
 scene(SV_ROOT,"root-top");sv_scroll[0]=sv_max_scroll();scene(SV_ROOT,"root-bottom");
 settings_render(0,SETTINGS_ROW_COUNT);capture("root-last-focused");
 assert(sv_row_y(SETTINGS_ROW_COUNT-1)-sv_scroll[0]+SV_ROW_HEIGHT<=174);
 settings_draft=stored;
 const char*messages[]={"Draft only - use SAVE","Invalid date or time","Write failed - check RTC","RTC saved; alarm sync failed","Verify failed - check RTC","Choose a valid Time Zone first"};
 for(unsigned m=0;m<sizeof(messages)/sizeof(*messages);++m){
  editor_message=messages[m];int origin=sv_fields_message_y(editor_message);
  if(side==240)assert(origin==185);
  else {int top=999,bottom=-999;for(unsigned i=0;editor_message[i];++i){const rps_glyph*g=sv_glyph(3,(unsigned char)editor_message[i]);if(!g->height)continue;if(g->top<top)top=g->top;if(g->top+g->height>bottom)bottom=g->top+g->height;}
   int bx=130,by=200,bw=86,bh=26;sv_native_button_geometry(&bx,&by,&bw,&bh);
   assert(sv_native_y(origin)+top>=sv_native_y(180));assert(sv_native_y(origin)+bottom<=sv_native_y(by));
   if(side>=328){assert(sv_native_y(origin)+top-sv_native_y(180)>=3);assert(sv_native_y(by)-sv_native_y(origin)-bottom>=3);}}
 }
 editor_message="Draft only - use SAVE";scene(SV_FIELDS,"time-date-top");sv_scroll[1]=sv_max_scroll();scene(SV_FIELDS,"time-date-bottom");
 for(unsigned f=0;f<6;f++){char name[32];editor_field=f;snprintf(name,sizeof(name),"time-field-%u",f);scene(SV_VALUE,name);}
 scene(SV_FOLD,"ambiguous-time");scene(SV_ABOUT,"about");scene(SV_TIME_FORMAT,"time-format");
 for(unsigned m=0;m<3;m++){char name[32];sleep_choice=m;snprintf(name,sizeof(name),"sleep-mode-%u",m);scene(SV_SLEEP,name);}
 sleep_message="Saving - not confirmed";scene(SV_SLEEP,"sleep-saving");sleep_message="Save unconfirmed - retry";scene(SV_SLEEP,"sleep-error");
 for(unsigned deep=0;deep<2;deep++){timer_deep=deep;timer_choice=3600000;timer_message="Saving - not confirmed";scene(SV_TIMER,deep?"deep-saving":"idle-saving");timer_message="Save unconfirmed - retry";scene(SV_TIMER,deep?"deep-error":"idle-error");}
 end();
 /* Actual app root touch, nested Time/Date Cancel, then root Back. */
 begin();polls=input_count=0;
 tap(3,60,sv_native_side()>240?78:144);tap(7,60,213);tap(11,120,205);
 app_main();assert(!record_writes[0]&&!record_writes[1]&&!record_writes[2]);end();
 puts("PASS native Settings layout edges/inverse map, minimum action bounds, full modeled page/editor states");
}

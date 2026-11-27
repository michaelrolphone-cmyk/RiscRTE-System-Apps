/* Native-layout boundary and modeled scene coverage. No hardware acceptance. */
#define PORTABLE_SETTINGS_BOTH_TIMERS
#define main inherited_geometry_main
#include "settings_sleep_defaults_test.c"
#undef main
static void capture(const char*name){
 unsigned outside=0;for(unsigned yy=0;yy<TEST_SETTINGS_RASTER_HEIGHT;++yy)for(unsigned xx=0;xx<TEST_SETTINGS_RASTER_WIDTH;++xx)if(framebuffer[yy*TEST_SETTINGS_RASTER_WIDTH+xx]&&!portable_round_contains(TEST_SETTINGS_RASTER_WIDTH,TEST_SETTINGS_RASTER_HEIGHT,xx,yy,0))++outside;
 if(outside)fprintf(stderr,"OUTSIDE %s %u\n",name,outside);
 assert(!outside);
 const char*dir=getenv("SETTINGS_NATIVE_FRAMES");if(!dir)return;
 char path[512];snprintf(path,sizeof(path),"%s/%s.ppm",dir,name);FILE*f=fopen(path,"wb");assert(f);
 fprintf(f,"P6\n%d %d\n255\n",TEST_SETTINGS_RASTER_WIDTH,TEST_SETTINGS_RASTER_HEIGHT);
 for(unsigned i=0;i<sizeof(framebuffer)/sizeof(*framebuffer);++i){unsigned v=framebuffer[i];unsigned char b[]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};assert(fwrite(b,1,3,f)==3);}fclose(f);
}
static void scene(unsigned page,const char*name){sv_page=page;settings_view_redraw();capture(name);}
int main(void){
 begin();int side=sv_native_side();
 int buttons[][4]={{24,200,86,26},{130,200,86,26},{82,192,76,26},{24,66,36,36},{180,66,36,36},{28,76,184,32},{28,124,184,32},{28,66,184,30},{28,104,184,30},{28,142,184,30},{20,88,64,44},{156,88,64,44}};
 for(unsigned b=0;b<sizeof(buttons)/sizeof(*buttons);++b){int bx=buttons[b][0],by=buttons[b][1],bw=buttons[b][2],bh=buttons[b][3];sv_native_button_geometry(&bx,&by,&bw,&bh);
  assert(sv_native_x(bx+bw)-sv_native_x(bx)>=44);assert(sv_native_y(by+bh)-sv_native_y(by)>=44);
  assert(portable_round_contains(width(),height(),sv_native_x(bx),sv_native_y(by),8));assert(portable_round_contains(width(),height(),sv_native_x(bx+bw)-1,sv_native_y(by+bh)-1,8));
 }
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
 for(int header_y=0;header_y<40;++header_y){uint32_t category=123;int32_t selected=456;
  assert(settings_touch(120,header_y,&category,&selected)==T5_APP_SETTING_NO_CHANGE);
  assert(category==123&&selected==456);
 }
 assert(sv_row_y(SETTINGS_ROW_COUNT-1)-sv_scroll[0]+SV_ROW_HEIGHT<=174);
 settings_draft=stored;
 const char*messages[]={"Draft only - use SAVE","Invalid date or time","Write failed - check RTC","RTC saved; alarm sync failed","Verify failed - check RTC","Choose a valid Time Zone first"};
 for(unsigned m=0;m<sizeof(messages)/sizeof(*messages);++m){
  editor_message=messages[m];int origin=sv_fields_message_y(editor_message);
  if(side==240)assert(origin==185);
  else {int top=999,bottom=-999;for(unsigned i=0;editor_message[i];++i){const rps_glyph*g=sv_glyph(3,(unsigned char)editor_message[i]);if(!g->height)continue;if(g->top<top)top=g->top;if(g->top+g->height>bottom)bottom=g->top+g->height;}
   int bx=130,by=200,bw=86,bh=26;sv_native_button_geometry(&bx,&by,&bw,&bh);
   assert(sv_native_y(origin)+top>=sv_native_y(sv_round()?SV_FIELD_BOTTOM:180));assert(sv_native_y(origin)+bottom<=sv_native_y(by));
   if(side>=328){assert(sv_native_y(origin)+top-sv_native_y(sv_round()?SV_FIELD_BOTTOM:180)>=3);assert(sv_native_y(by)-sv_native_y(origin)-bottom>=3);}}
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
 puts("PASS round Settings complete visible pages, circle containment, controls, inverse hits and draft navigation");
}


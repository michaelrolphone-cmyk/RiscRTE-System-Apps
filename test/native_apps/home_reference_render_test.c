/* Actual foreground render helpers and hit targets, with copied catalog data. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../Apps/paper_clock.c"
static uint8_t pixels[480*800];
static int width(void){return 480;}static int height(void){return 800;}
static void fill(int x,int y,int w,int h,bool black){assert(x>=0&&y>=0&&w>=0&&h>=0&&x+w<=480&&y+h<=800);for(int j=y;j<y+h;j++)for(int i=x;i<x+w;i++)pixels[j*480+i]=black;}
static void save(const char *out,const char *name){char path[512];snprintf(path,sizeof(path),"%s/%s.pbm",out,name);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P4\n480 800\n");for(unsigned i=0;i<sizeof(pixels);i+=8){uint8_t b=0;for(unsigned j=0;j<8;j++)b|=pixels[i+j]<<(7-j);assert(fwrite(&b,1,1,f)==1);}assert(!fclose(f));}
static void render(const char *out,const char *name,const twatch_rtc_time_v1 *time,const home_point_state *state){memset(pixels,0,sizeof(pixels));home_press_begin();home_reference_header(time,true);home_reference_points(state,format,NULL);save(out,name);}
int main(int argc,char **argv){assert(argc==2);static const t5_app_api_v1 api={.screen_width=width,.screen_height=height,.fill_rect=fill};app=&api;
 battery_status=(paper_battery){.valid=true,.percent=84};format=PORTABLE_TIME_FORMAT_24;
 twatch_rtc_time_v1 time={.year=2026,.month=10,.day=9,.weekday=5,.hour=11,.minute=40};
 home_point_state state={.status=NOVA_POINTS_READY,.now_rtc=100000,.next_count=4,.previous_valid=1};
 state.previous.at_rtc=94900;
 const char *names[]={"LUNCH","BREAK","END WORK","DINNER"};unsigned hours[]={12,15,17,18};unsigned minutes[]={20,200,320,410};
 for(unsigned i=0;i<4;i++){state.next[i].hour=hours[i];state.next[i].minute=i==3?30:0;state.next[i].at_rtc=state.now_rtc+minutes[i]*60;strcpy(state.next[i].label,names[i]);}
 render(argv[1],"home-reference-24h",&time,&state);
 uint8_t normal[sizeof(pixels)];memcpy(normal,pixels,sizeof(pixels));
 const unsigned targets[]={HOME_TOP,HOME_DIAL,HOME_POINTS,HOME_FILES,HOME_DOCK_POINTS,HOME_CONTEXTS,HOME_SETTINGS};
 for(unsigned i=0;i<sizeof(targets)/sizeof(targets[0]);i++){
  unsigned target=targets[i];int x,y,w,h;home_press_bounds(target,&x,&y,&w,&h);assert(home_target(x+w/2,y+h/2)==target);
  home_pressed=target;char name[40];snprintf(name,sizeof(name),"home-press-%u",target);render(argv[1],name,&time,&state);
  for(unsigned row=0;row<800;row++)for(unsigned col=0;col<480;col++)assert(pixels[row*480+col]==(normal[row*480+col]^(col>=(unsigned)x&&col<(unsigned)(x+w)&&row>=(unsigned)y&&row<(unsigned)(y+h))));
 }
 home_pressed=HOME_NONE;
 assert(!strcmp(home_launch_target(HOME_FILES),"file_browser.elf"));assert(!strcmp(home_launch_target(HOME_DOCK_POINTS),"points_in_time.elf"));assert(!strcmp(home_launch_target(HOME_CONTEXTS),"contexts.elf"));assert(!strcmp(home_launch_target(HOME_SETTINGS),"settings.elf"));
 assert(home_target(31,690)==HOME_NONE&&home_target(448,690)==HOME_NONE&&home_target(200,630)==HOME_NONE);
 assert(home_target(135,690)==HOME_FILES&&home_target(136,690)==HOME_DOCK_POINTS);
 assert(home_target(239,690)==HOME_DOCK_POINTS&&home_target(240,690)==HOME_CONTEXTS);
 for(unsigned c=33;c<127;c++){char label[32],lines[3][32];memset(label,c,31);label[31]=0;unsigned count=home_reference_lines(&HOME_REF_FOLLOW,label,198,lines);if(!count)count=home_reference_lines(&HOME_REF_DOCK,label,198,lines);assert(count&&count<=3);unsigned n=0;for(unsigned j=0;j<count;j++)n+=strlen(lines[j]);assert(n==31);}
 strcpy(state.next[0].label,"A long catalog title with words");strcpy(state.next[1].label,"WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW");strcpy(state.next[2].label,"Following complete catalog name");render(argv[1],"home-full-labels",&time,&state);
 format=PORTABLE_TIME_FORMAT_12;time.hour=0;time.minute=5;state.next[0].hour=0;render(argv[1],"home-midnight-12h",&time,&state);
 format=PORTABLE_TIME_FORMAT_24;render(argv[1],"home-midnight-24h",&time,&state);
 state.status=NOVA_POINTS_EMPTY;state.next_count=0;render(argv[1],"home-empty",&time,&state);
 state.status=NOVA_POINTS_UNAVAILABLE;render(argv[1],"home-unavailable",&time,&state);
 puts("Home reference renderer: native fonts, full labels, 12/24, seven targets, exact press inversion and dock launches PASS");return 0;}

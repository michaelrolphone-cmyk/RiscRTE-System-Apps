/* Foreground Points presentation and copied-window refresh for sparse timers. */
#if !defined(PORTABLE_DESK_CLOCK_SPARSE_START) || !defined(ALARM_NATIVE_UTC)
#error "Home Points requires the native sparse Clock profile"
#endif
#ifdef PORTABLE_DESK_POINTS_SNAPSHOT
#include "PaperHomeCatalogPoints.h"
#include "PortableHomePointsCatalog.h"
typedef paper_catalog_state home_point_state;
typedef paper_catalog_event home_point_event;
#else
#include "PaperHomePoints.h"
typedef nova_points_state home_point_state;
typedef nova_point_event home_point_event;
#endif
#include "PaperQuickIntent.h"
#include "paper_home_type.inc"
#ifndef PAPER_POINTS_APP
#define PAPER_POINTS_APP "points_in_time.elf"
#endif
static home_point_state home_points;
#ifdef PORTABLE_DESK_POINTS_SNAPSHOT
static portable_desk_points_snapshot home_points_snapshot;
static uint32_t home_points_polled_at;
static bool home_points_refresh(bool known) {
 if(!desk_promoted||desk_held||desk_retained||portable_app_sleep_retained())return false;
 home_point_state next={.status=NOVA_POINTS_UNAVAILABLE};
 portable_desk_points_snapshot snapshot={0};
 if(known&&desk_home_valid&&desk_zone_known) {
  /* This copies service-owned memory only. Reconciliation and its storage/RF
   * policy remain in the existing foreground alarm pump. */
  portable_points_catalog_view view={.struct_size=sizeof(view)};
  int status=portable_home_points_catalog(&view);
  if(status==-2 || desk_retained || portable_app_sleep_retained()){desk_fence();return false;}
  if(status==1 && portable_points_catalog_view_valid(&view)) {
   snapshot=(portable_desk_points_snapshot){.status=PORTABLE_DESK_POINTS_READY,.view=view};
   (void)paper_catalog_project(&view,&desk_rule,INT64_C(946684800)+view.seconds,&next);
  } else if(status==2) {
   next.status=NOVA_POINTS_PENDING;
  } else if(status!=0) {
   next.status=NOVA_POINTS_ERROR;snapshot.status=PORTABLE_DESK_POINTS_ERROR;
  }
 }
 home_points=next;home_points_snapshot=snapshot;home_points_polled_at=app->millis();
 return true;
}
/* The service owns reconciliation. Home only samples its copied projection,
 * at most four times a second and only with a settled display. A refresh that
 * was busy when readiness changed is retried as soon as it settles. Snapshot
 * churn alone must not turn normal service evaluation into panel refreshes. */
static bool home_points_event_equal(const home_point_event *a,const home_point_event *b) {
 return a->event_id==b->event_id && a->type_id==b->type_id && a->revision==b->revision &&
  a->at_rtc==b->at_rtc && a->parent_day==b->parent_day && a->color==b->color &&
  a->hour==b->hour && a->minute==b->minute && a->edge==b->edge && a->mode==b->mode &&
  a->day_offset==b->day_offset && !strcmp(a->label,b->label);
}
static bool home_points_poll(bool known,uint32_t now,bool *changed) {
 *changed=false;
 if(desk_retained||portable_app_sleep_retained())return false;
 if((uint32_t)(now-home_points_polled_at)<250u)return true;
 if(!paper_frame_ready())return !desk_retained&&!portable_app_sleep_retained();
 home_point_state previous=home_points;
 uint32_t generation=home_points_snapshot.view.snapshot;
 if(!home_points_refresh(known))return false;
 home_refresh_pending=false;
 *changed=previous.status!=home_points.status;
 if(!*changed && generation==home_points_snapshot.view.snapshot)return true;
 *changed|=previous.revision!=home_points.revision || previous.next_count!=home_points.next_count ||
  previous.previous_valid!=home_points.previous_valid;
 if(home_points.previous_valid)*changed|=!home_points_event_equal(&previous.previous,&home_points.previous);
 for(unsigned i=0;i<home_points.next_count;i++)
  *changed|=!home_points_event_equal(&previous.next[i],&home_points.next[i]);
 return true;
}
#else
static bool home_points_status(int32_t status) {
 if(desk_retained||portable_app_sleep_retained())return false;
 if(status!=RISC_KEY_VALUE_OK && status!=RISC_KEY_VALUE_NOT_FOUND &&
    status!=RISC_KEY_VALUE_BUFFER_SMALL && status!=RISC_KEY_VALUE_INVALID && status!=RISC_KEY_VALUE_IO) {
  desk_fence();return false;
 }
 return true;
}
static bool home_points_refresh(bool known) {
 home_points=(nova_points_state){.status=NOVA_POINTS_UNAVAILABLE};
 if(!desk_promoted||desk_held||desk_retained||portable_app_sleep_retained())return false;
 if(!known||!desk_home_valid||!desk_zone_known)return true;
#ifdef PORTABLE_CONTEXTS_CLOCK_RF_ONLY
 if(!portable_contexts_stop()){desk_fence();return false;}
#endif
 risc_runtime_capability_v1 grant={.struct_size=sizeof(grant)};
 if(!rt->acquire(RISC_KEY_VALUE_CAPABILITY,1,5,&grant)){desk_fence();return false;}
 if(desk_retained||portable_app_sleep_retained())return false;
 if(!grant.api||!grant.slot||!grant.generation){desk_fence();return false;}
 const risc_key_value_v1 *kv=grant.api;points_config config={0};points_meta meta={0};bool loaded=false;
 if(kv->api_version==1&&kv->struct_size>=sizeof(*kv)&&kv->get) {
  uint8_t bytes[POINTS_RECORD_SIZE];uint32_t length=0;
  int32_t result=kv->get(kv->context,POINTS_CONFIG_KEY,bytes,sizeof(bytes),&length);
  if(!home_points_status(result))return false;
  bool defaults=result==RISC_KEY_VALUE_NOT_FOUND;
  if(defaults){config=points_default_config();loaded=true;}
  else loaded=result==RISC_KEY_VALUE_OK&&points_config_decode(&config,bytes,length);
  if(loaded) {
   length=0;result=kv->get(kv->context,POINTS_META_KEY,bytes,sizeof(bytes),&length);
   if(!home_points_status(result))return false;
   if(defaults&&result==RISC_KEY_VALUE_NOT_FOUND)meta=points_default_meta();
   else if(result==RISC_KEY_VALUE_OK&&!points_meta_decode(&meta,bytes,length))meta=(points_meta){0};
  }
 }
 if(!rt->release(&grant)||grant.api||grant.slot||grant.generation){desk_fence();return false;}
 if(desk_retained||portable_app_sleep_retained())return false;
 if(loaded)(void)paper_points_project(&config,&meta,&desk_rule,desk_home_epoch,&home_points);
 else home_points.status=NOVA_POINTS_ERROR;
 return true;
}
#endif
static void home_duration(char *out,size_t size,uint32_t seconds) {
 uint32_t minutes=(seconds+59u)/60u;
 if(minutes>=1440)snprintf(out,size,"IN %lu D %lu H",(unsigned long)(minutes/1440),(unsigned long)(minutes%1440/60));
 else if(minutes>=60)snprintf(out,size,"IN %lu H %02lu M",(unsigned long)(minutes/60),(unsigned long)(minutes%60));
 else snprintf(out,size,"IN %lu MIN",(unsigned long)minutes);
}
static void home_time(char *out,size_t size,const home_point_event *event,bool next,unsigned format) {
 unsigned hour=format==PORTABLE_TIME_FORMAT_24?event->hour:event->hour%12?event->hour%12:12;
 snprintf(out,size,"%s%u:%02u%s",next?"AT ":"",hour,event->minute,
  format==PORTABLE_TIME_FORMAT_24?"":event->hour<12?" AM":" PM");
}
#ifdef PORTABLE_DESK_POINTS_SNAPSHOT
/* Measured wrapping keeps all 31 printable label bytes, including an
 * unbroken word. Every line is checked against the actual glyph advances. */
static bool home_label_lines(const home_font *font,const char *label,int maximum,char lines[2][32]) {
 size_t length=strlen(label);if(length>31)return false;
 memset(lines,0,64);
 if(home_text_width(font,label,0)<=maximum*64){memcpy(lines[0],label,length+1);return true;}
 unsigned split=0,last_space=0;
 for(unsigned i=1;i<length;i++) {
  memcpy(lines[0],label,i);lines[0][i]=0;
  if(home_text_width(font,lines[0],0)>maximum*64)break;
  split=i;if(label[i]==' ')last_space=i;
 }
 if(last_space) {
  unsigned remainder=last_space;while(label[remainder]==' ')remainder++;
  if(home_text_width(font,label+remainder,0)<=maximum*64)split=last_space;
 }
 if(!split)return false;
 memcpy(lines[0],label,split);lines[0][split]=0;
 unsigned next=split;while(label[next]==' ')next++;
 snprintf(lines[1],32,"%s",label+next);
 return home_text_width(font,lines[1],0)<=maximum*64;
}
static void home_catalog_label(const char *label,int x,int baseline,int maximum,bool headline) {
 char lines[2][32];const home_font *font=headline?&HOME_O900_52:&HOME_R700_22;int gap=headline?36:19;
 if(headline && home_text_width(font,label,0)>maximum*64)font=&HOME_R700_30;
 if(!home_label_lines(font,label,maximum,lines)) {
  font=&HOME_R700_26;
  if(!home_label_lines(font,label,maximum,lines))return;
 }
 home_type(font,x,baseline,lines[0],0,0,0,0);
 if(lines[1][0])home_type(font,x,baseline+gap,lines[1],0,0,0,0);
}
static inline void home_points_render(const home_point_state *state,unsigned format,const char *notice) {
 char value[48];
 if(state->status!=NOVA_POINTS_READY||!state->next_count) {
  home_type(&HOME_R700_30,32,568,state->status==NOVA_POINTS_EMPTY?"NO POINTS":state->status==NOVA_POINTS_PENDING?"LOADING POINTS":"POINTS UNAVAILABLE",0,0,416,0);
  home_type(&HOME_R700_26,32,608,state->status==NOVA_POINTS_EMPTY?"TAP TO ADD ONE":state->status==NOVA_POINTS_PENDING?"PLEASE WAIT":"OPEN POINTS TO CHECK",0,0,416,0);
 } else {
  const home_point_event *next=&state->next[0];
  home_type(&HOME_R700_22,32,508,"NEXT",8,0,0,0);
  home_time(value,sizeof(value),next,true,format);home_type(&HOME_O700_24,448,508,value,0,1,250,0);
  home_catalog_label(next->label,32,552,416,true);
  home_duration(value,sizeof(value),next->at_rtc-state->now_rtc);
  home_type(&HOME_R700_26,32,620,value,0,0,416,0);
  home_round(30,634,420,20,5,true);home_round(34,638,412,12,2,false);
  unsigned progress=paper_catalog_progress(state,416);
  if(state->previous_valid&&progress<8)progress=8;
  if(progress)home_round(30,634,(int)progress+4,20,5,true);
  for(unsigned i=1;i<state->next_count;i++) {
   const home_point_event *event=&state->next[i];int baseline=680+(int)(i-1)*42;
   home_time(value,sizeof(value),event,false,format);
   home_type(&HOME_O700_24,32,baseline,value,0,0,120,0);
   home_catalog_label(event->label,164,baseline,284,false);
  }
 }
 if(notice&&*notice)home_type(&HOME_R600_22,32,480,notice,0,0,416,0);
}
#else
static inline void home_points_render(const home_point_state *state,unsigned format,const char *notice) {
 char value[48];
 if(state->status!=NOVA_POINTS_READY||!state->next_count) {
  home_type(&HOME_O900_52,32,568,state->status==NOVA_POINTS_EMPTY?"NO POINTS":"POINTS UNAVAILABLE",0,0,416,0);
  home_type(&HOME_R700_26,32,608,state->status==NOVA_POINTS_EMPTY?"TAP TO ADD ONE":"OPEN POINTS TO CHECK",2,0,416,0);
 } else {
  const home_point_event *next=&state->next[0];
  home_type(&HOME_R700_22,32,508,"NEXT",8,0,0,0);
  home_time(value,sizeof(value),next,true,format);home_type(&HOME_O700_24,448,508,value,0,1,250,0);
  home_type(&HOME_O900_52,32,568,paper_points_label(next),0,0,416,home_svg_fit(paper_points_label(next),52,86,0,416));
  home_duration(value,sizeof(value),next->at_rtc-state->now_rtc);
  home_type(&HOME_R700_30,32,608,value,2,0,416,0);
  home_round(30,624,420,28,6,true);home_round(34,628,412,20,2,false);
  unsigned progress=paper_points_progress(state,416);
  if(state->previous_valid&&progress<8)progress=8;
  if(progress)home_round(30,624,(int)progress+4,28,6,true);
  for(unsigned i=1;i<state->next_count&&i<3;i++) {
   const home_point_event *event=&state->next[i];int baseline=692+(int)(i-1)*38;
   home_time(value,sizeof(value),event,false,format);
   home_type(&HOME_O700_28,32,baseline,value,0,0,120,0);
   home_type(&HOME_R700_28,164,baseline,paper_points_label(event),2,0,150,home_svg_fit(paper_points_label(event),28,52,2,150));
   home_duration(value,sizeof(value),event->at_rtc-state->now_rtc);
   home_type(&HOME_R600_22,448,baseline,value,0,1,130,home_svg_fit(value,22,52,0,130));
  }
 }
 if(notice&&*notice)home_type(&HOME_R600_22,32,778,notice,0,0,416,0);
}
#endif
#include "paper_home_reference.inc"
static void home_points_draw(const char *notice) {home_reference_points(&home_points,format,notice);}
static bool home_points_hit(int x,int y) {
 return x>=xscale(24)&&x<xscale(456)&&y>=yscale(254)&&y<yscale(618);
}
static bool home_dial_hit(int x,int y) {
 return x>=xscale(24)&&x<xscale(456)&&y>=yscale(72)&&y<yscale(246);
}
static inline bool home_top_hit(int x,int y) {
 return x>=0&&x<xscale(480)&&y>=0&&y<yscale(72);
}
static unsigned home_target(int x,int y) {
 if(home_top_hit(x,y))return HOME_TOP;
 if(y>=yscale(634)&&y<yscale(778)&&x>=xscale(32)&&x<xscale(448)) {
  for(unsigned i=0;i<4;i++)if(x<xscale(32+(int)(i+1)*104))return HOME_FILES+i;
 }
 if(home_dial_hit(x,y))return HOME_DIAL;
 if(home_points_hit(x,y))return HOME_POINTS;
 return HOME_NONE;
}

#ifdef PORTABLE_DESK_POINTS_FACE
/* TIMER already owns alarm.service and its dependency closure. Projection uses
 * that admitted grant without Runtime promotion or foreground adapter upgrade:
 * no Home, preferences, input, radio policy or wake-light restoration. */
static int desk_points_refresh_timer(portable_desk_config *config,int64_t epoch,bool rewound) {
 if(config->face!=PORTABLE_DESK_POINTS)return 0;
 if(desk_held || desk_retained || portable_app_sleep_retained())return -2;
 uint32_t seconds;
 if(!rewound && config->points.status==PORTABLE_DESK_POINTS_READY &&
    points_utc_from_unix(epoch,&seconds) && seconds>=config->points.view.seconds &&
    seconds<config->points.view.valid_until)return 0;
 portable_desk_points_snapshot next={0};
 next.view.struct_size=sizeof(next.view);
 int status=portable_desk_points_catalog_refresh(&next.view);
 if(status==-2 || desk_retained || portable_app_sleep_retained())return -2;
 if(status==1 && portable_points_catalog_view_valid(&next.view))
  next.status=PORTABLE_DESK_POINTS_READY;
 else {
  next=(portable_desk_points_snapshot){.status=status==0?
      PORTABLE_DESK_POINTS_UNAVAILABLE:PORTABLE_DESK_POINTS_ERROR};
 }
 config->points=next;
 return 1;
}
static bool desk_points_capture(portable_desk_config *config) {
 twatch_rtc_time_v1 time;bool known=read_clock(&time);
 if(!home_points_refresh(known))return false;
 config->points=home_points_snapshot;return true;
}
static bool desk_points_scene(const portable_desk_config *config,int64_t minute) {
 twatch_rtc_time_v1 time={0};portable_timezone_rule rule;paper_catalog_state state={.status=NOVA_POINTS_UNAVAILABLE};
 bool valid=minute>=PORTABLE_TIMEZONE_VALID_EPOCH && desk_local(config,minute,&time);
 if(!portable_desk_adapter_landscape(true,config->flip_ui))return false;
 portable_desk_adapter_begin();desk_rectangles=0;home_desk_ok=true;home_desk_clock=true;
 home_fill(0,0,app->screen_width(),app->screen_height(),false);
 char value[64];
 if(valid)snprintf(value,sizeof(value),"%04u-%02u-%02u",time.year,time.month,time.day);
 else strcpy(value,"TIME UNAVAILABLE");
 home_type(&HOME_R700_22,164,132,value,0,2,300,0);
 if(valid)snprintf(value,sizeof(value),"%u:%02u",config->time_format==PORTABLE_TIME_FORMAT_24?
  time.hour:time.hour%12?time.hour%12:12,time.minute);
 else strcpy(value,"--:--");
 home_type(&HOME_DESK_TIME,164,260,value,0,2,0,0);
 if(valid && config->time_format==PORTABLE_TIME_FORMAT_12)
  home_type(&HOME_R700_22,164,318,time.hour<12?"AM":"PM",0,2,0,0);
 home_desk_clock=false;home_desk_widget=true;
 if(config->points.status==PORTABLE_DESK_POINTS_READY && valid &&
    portable_timezone_resolve(config->time_zone,sizeof(config->time_zone),&rule)==PORTABLE_TIMEZONE_OK)
 {
  /* The retained image minute can precede capture by up to59 seconds. Reuse
   * that exact captured second for its original minute so old-image seeding
   * reconstructs the same labels/countdown; earlier minutes still fail. */
  int64_t captured=INT64_C(946684800)+config->points.view.seconds,project_at=minute;
  if(project_at<captured && captured-project_at<60)project_at=captured;
  (void)paper_catalog_project(&config->points.view,&rule,project_at,&state);
 }
 else if(config->points.status==PORTABLE_DESK_POINTS_ERROR)state.status=NOVA_POINTS_ERROR;
 home_points_render(&state,config->time_format,NULL);
 home_desk_widget=false;return home_desk_ok && !desk_cancelled();
}
#endif

#include "PortableSettingsScene.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern bool settings_scene_production_valid(const risc_components_document_v1 *);
static unsigned puts_count;
static bool live(void *p){(void)p;return true;}
static int32_t get(void *p,const char *key,void *out,uint32_t cap,uint32_t *size){(void)p;(void)key;(void)out;(void)cap;*size=0;return RISC_KEY_VALUE_NOT_FOUND;}
static int32_t put(void *p,const char *key,const void *data,uint32_t size){(void)p;(void)key;(void)data;(void)size;puts_count++;return RISC_KEY_VALUE_OK;}
static int begin(void *p){(void)p;return PSM_OK;}
static int read_clock(void *p,portable_timezone_civil *c){(void)p;*c=(portable_timezone_civil){2026,11,4,12,30,0,0};return PSM_OK;}
static int start(void *p,const portable_set_time_request *r,portable_set_time_result *v){(void)p;(void)r;(void)v;return PSM_PENDING;}
static int poll_clock(void *p,portable_set_time_result *v){(void)p;(void)v;return PSM_OK;}
static risc_key_value_v1 kv={1,sizeof(kv),NULL,get,put};
static void init(PortableSettingsModel *m,pss_scene *s){psm_policy p=psm_default_policy();psm_services v={.live=live,.clock_begin=begin,.clock_read=read_clock,.clock_start=start,.clock_poll=poll_clock,.clock_cancel=poll_clock};assert(psm_init(m,&p,&v,&kv,NULL));pss_init(s,m);}
static const risc_scene_node_v1 *label(const risc_components_document_v1 *d,const char *name){for(unsigned i=0;i<d->node_count;i++)if(!strcmp(d->nodes[i].label,name))return &d->nodes[i];return NULL;}
static bool hit(pss_scene *s,const risc_components_document_v1 *d,const risc_scene_node_v1 *n,int32_t value){assert(n);risc_scene_event_v1 e={.struct_size=sizeof(e),.kind=(n->kind==RISC_COMPONENT_STEPPER||n->kind==RISC_COMPONENT_CHECK_ROW)?RISC_SCENE_VALUE_EVENT:RISC_SCENE_ACTION_EVENT,.document_revision=d->revision,.node=n->id,.action=n->action,.value=value};return pss_action(s,&e);}
static void valid(const risc_components_document_v1 *d){if(!settings_scene_production_valid(d))fprintf(stderr,"Invalid production document: %s, %u nodes, screen %u\n",d->routes[0].title,d->node_count,d->screen_key);assert(settings_scene_production_valid(d));assert(d->node_count>0&&d->node_count<=96&&d->route_count==1);for(unsigned i=0;i<d->node_count;i++){assert(d->nodes[i].route==d->root);assert(d->nodes[i].id);if(d->nodes[i].kind==RISC_COMPONENT_STEPPER){char formatted[32];snprintf(formatted,sizeof(formatted),"%ld",(long)d->nodes[i].value);assert(!strcmp(d->nodes[i].text,formatted));}for(unsigned j=0;j<i;j++)assert(d->nodes[i].id!=d->nodes[j].id);}}
static void catalog(void){PortableSettingsModel m;pss_scene s;init(&m,&s);assert(psm_open(&m,PSM_ZONE));unsigned found=0,max_nodes=0;bool seen[500]={false};risc_components_document_v1 d;
 assert(portable_timezone_count()==419);
 for(unsigned r=0;r<PORTABLE_TIMEZONE_REGION_COUNT;r++){
  s.zone_region=-1;s.zone_initial=-1;assert(pss_build(&s,&d));assert(hit(&s,&d,label(&d,portable_timezone_region_name(r)),0));assert(pss_build(&s,&d));
  if(portable_timezone_region_count(r)>64){risc_scene_node_v1 groups[27];unsigned groups_n=0;for(unsigned i=0;i<d.node_count;i++)if(d.nodes[i].action>=600&&d.nodes[i].action<627)groups[groups_n++]=d.nodes[i];
   for(unsigned g=0;g<groups_n;g++){s.zone_initial=-1;assert(pss_build(&s,&d));assert(hit(&s,&d,label(&d,groups[g].label),0));assert(pss_build(&s,&d));valid(&d);if(d.node_count>max_nodes)max_nodes=d.node_count;
    for(unsigned i=0;i<d.node_count;i++)if(d.nodes[i].action>=1000&&d.nodes[i].action<1500){unsigned index=d.nodes[i].action-1000;assert(!seen[index]);seen[index]=true;found++;assert(d.nodes[i].label[0]);}}
  }else{valid(&d);if(d.node_count>max_nodes)max_nodes=d.node_count;for(unsigned i=0;i<d.node_count;i++)if(d.nodes[i].action>=1000&&d.nodes[i].action<1500){unsigned index=d.nodes[i].action-1000;assert(!seen[index]);seen[index]=true;found++;}}
 }
 assert(found==419);printf("Catalog: %u reachable unique zones, largest document %u nodes\n",found,max_nodes);
 /* Select each entry through its emitted event, not a private direct setter. */
 for(unsigned z=0;z<419;z++){const portable_timezone_entry *e=portable_timezone_get(z);s.zone_region=e->region;const char *p=strchr(e->id,'/');p=p?p+1:e->id;s.zone_initial=*p-'A';assert(pss_build(&s,&d));const risc_scene_node_v1 *choice=NULL;for(unsigned i=0;i<d.node_count;i++)if(d.nodes[i].action==1000+z)choice=&d.nodes[i];assert(hit(&s,&d,choice,0));assert(!strcmp(m.draft.zone,e->id));}
 assert(!puts_count);
}
static void routes(void){PortableSettingsModel m;pss_scene s;risc_components_document_v1 d;init(&m,&s);assert(pss_build(&s,&d));valid(&d);assert(!pss_needs_publish(&s));
 for(unsigned f=0;f<PSM_FEATURE_COUNT;f++){m.route=PSM_ROOT;m.phase=PSM_IDLE;m.clock_active=false;m.atomic.valid=false;assert(psm_open(&m,(psm_feature)f));assert(pss_build(&s,&d));valid(&d);assert(label(&d,"Cancel"));
  if(m.route==PSM_EDITOR){assert(label(&d,"Save"));assert(label(&d,"Discard draft"));}
  if(f==PSM_TIME){const char *fields[]={"Year","Month","Day","Hour","Minute","Second"};for(unsigned i=0;i<6;i++)assert(label(&d,fields[i]));assert(hit(&s,&d,label(&d,"Minute"),31));assert(m.draft.civil.minute==31);}
  if(f==PSM_ALERT){m.draft_valid=false;assert(pss_build(&s,&d));assert(!m.draft_valid);assert(label(&d,"Save")->flags&RISC_SCENE_DISABLED);for(unsigned i=1;i<=3;i++){assert(pss_build(&s,&d));assert(hit(&s,&d,label(&d,psm_choice_label(PSM_ALERT,i)),0));assert(m.draft.number==i);}}
  if(f==PSM_IDLE_TIMER||f==PSM_DEEP_TIMER){const risc_scene_node_v1 *timer=label(&d,"Seconds");assert(timer&&timer->step==1&&timer->maximum==3600);assert(hit(&s,&d,timer,3600));assert(m.draft.number==3600000);}
  if(f==PSM_LANGUAGE||f==PSM_FACE){unsigned n=0;for(unsigned i=0;i<d.node_count;i++)n+=d.nodes[i].kind==RISC_COMPONENT_CHECK_ROW;assert(n==psm_choice_count((psm_feature)f));}
 }
 m.editing=PSM_TIME;m.draft.civil=(portable_timezone_civil){2026,11,4,12,30,0,0};m.route=PSM_FOLD;m.phase=PSM_IDLE;assert(pss_build(&s,&d));assert(label(&d,"Save")->flags&RISC_SCENE_DISABLED);assert(hit(&s,&d,label(&d,"Later occurrence"),0));assert(m.fold_choice==1&&m.route==PSM_EDITOR);
 m.clock_result.rtc_outcome=PORTABLE_SET_TIME_CONFIRMED;m.clock_result.metadata_outcome=PORTABLE_SET_TIME_UNCONFIRMED;m.clock_result.native.attempted=true;assert(pss_build(&s,&d));assert(!strcmp(label(&d,"RTC clock")->text,"Confirmed"));assert(!strcmp(label(&d,"System clock")->text,"Unconfirmed"));assert(!strcmp(label(&d,"RTC metadata")->text,"Unconfirmed"));
 for(unsigned phase=PSM_WAIT_PRESENTATION;phase<=PSM_RETAINED;phase++){m.phase=phase;assert(pss_build(&s,&d));valid(&d);unsigned n=0;for(unsigned i=0;i<d.node_count;i++)if(d.nodes[i].kind==RISC_COMPONENT_PROGRESS)n++;assert(n==1);assert(!label(&d,"Save")&&d.node_count==3);assert(d.screen_key==10000000+phase);}
}
static void fencing(void){PortableSettingsModel m;pss_scene s;risc_components_document_v1 d;init(&m,&s);assert(psm_open(&m,PSM_FORMAT));assert(pss_build(&s,&d));risc_scene_node_v1 n=*label(&d,"24-hour");
 risc_scene_event_v1 e={.struct_size=sizeof(e),.kind=RISC_SCENE_ACTION_EVENT,.document_revision=d.revision,.node=n.id,.action=n.action};e.document_revision--;assert(!pss_action(&s,&e));e.document_revision++;e.node++;assert(!pss_action(&s,&e));e.node--;e.kind=RISC_SCENE_SUSPEND_EVENT;assert(!pss_action(&s,&e));e.kind=RISC_SCENE_ACTION_EVENT;assert(pss_action(&s,&e));assert(!pss_action(&s,&e));assert(pss_needs_publish(&s));assert(pss_build(&s,&d));assert(hit(&s,&d,label(&d,"Save"),0));assert(!puts_count);assert(pss_build(&s,&d));assert(pss_save_revision(&s,d.revision)==m.save_revision);assert(!pss_save_revision(&s,d.revision-1));assert(!puts_count);
 pss_scene candidate=s;risc_components_document_v1 next;assert(pss_build(&candidate,&next));assert(s.revision==d.revision);assert(candidate.revision>d.revision);assert(psm_presented(&m,pss_save_revision(&s,d.revision)));assert(!pss_save_revision(&s,d.revision));assert(pss_needs_publish(&s));assert(!hit(&s,&d,label(&d,"Cancel operation"),0));psm_tick(&m);assert(puts_count==1);
}
static void cancellation_and_policy(void){
 PortableSettingsModel m;pss_scene s;risc_components_document_v1 d;init(&m,&s);
 assert(pss_build(&s,&d));risc_scene_event_v1 back={.struct_size=sizeof(back),.kind=RISC_SCENE_ACTION_EVENT,.document_revision=d.revision,.action=d.routes[0].back_action};assert(pss_action(&s,&back));assert(s.exit_requested);
 init(&m,&s);assert(psm_open(&m,PSM_FORMAT));assert(pss_build(&s,&d));assert(hit(&s,&d,label(&d,"Save"),0));assert(pss_build(&s,&d));uint32_t saving=d.revision;assert(pss_save_revision(&s,saving));assert(hit(&s,&d,label(&d,"Cancel operation"),0));assert(!pss_save_revision(&s,saving));assert(m.route==PSM_ROOT&&m.phase==PSM_IDLE);
 init(&m,&s);m.policy.desk_lock=true;assert(psm_open(&m,PSM_SLEEP));assert(pss_build(&s,&d));assert(label(&d,"Save")->flags&RISC_SCENE_DISABLED);for(unsigned i=0;i<d.node_count;i++)if(d.nodes[i].kind==RISC_COMPONENT_CHECK_ROW){assert(d.nodes[i].flags&RISC_SCENE_DISABLED);assert(!hit(&s,&d,&d.nodes[i],0));}
 init(&m,&s);m.policy.sleep_modes=5;assert(psm_open(&m,PSM_SLEEP));assert(pss_build(&s,&d));assert(label(&d,"Light"));assert(!label(&d,"Deep"));assert(label(&d,"Light then Deep"));
 init(&m,&s);assert(psm_open(&m,PSM_FORMAT));m.uncertain=true;m.status[PSM_FORMAT]=PSM_UNCONFIRMED;m.draft.number=1;m.dirty=true;assert(pss_build(&s,&d));assert(hit(&s,&d,label(&d,"Reload preconditions"),0));assert(m.draft.number==1&&m.dirty);assert(m.phase==PSM_IDLE);
 init(&m,&s);assert(pss_build(&s,&d));uint32_t before=m.revision;assert(hit(&s,&d,label(&d,"Reload"),0));assert(s.reload_requested&&m.revision==before);
 init(&m,&s);assert(psm_open(&m,PSM_ZONE));assert(pss_build(&s,&d));assert(hit(&s,&d,label(&d,"America"),0));assert(pss_build(&s,&d));uint32_t r=d.revision;assert(hit(&s,&d,label(&d,"A"),0));assert(pss_build(&s,&d));assert(d.revision>r);back.document_revision=d.revision;back.action=d.routes[0].back_action;assert(pss_action(&s,&back));assert(s.zone_region==PORTABLE_TIMEZONE_AMERICA&&s.zone_initial==-1&&m.route==PSM_EDITOR);
}
static void twelve_hour(void){PortableSettingsModel m;pss_scene s;risc_components_document_v1 d;init(&m,&s);assert(psm_open(&m,PSM_TIME));assert(pss_build(&s,&d));valid(&d);assert(label(&d,"Hour")->minimum==1&&label(&d,"Hour")->maximum==12&&label(&d,"Hour")->value==12);assert(label(&d,"PM")->value==1);
 assert(hit(&s,&d,label(&d,"AM"),1));assert(m.draft.civil.hour==0);assert(pss_build(&s,&d));assert(hit(&s,&d,label(&d,"Hour"),12));assert(m.draft.civil.hour==0);assert(pss_build(&s,&d));assert(hit(&s,&d,label(&d,"Hour"),1));assert(m.draft.civil.hour==1);assert(pss_build(&s,&d));assert(hit(&s,&d,label(&d,"PM"),1));assert(m.draft.civil.hour==13);assert(pss_build(&s,&d));assert(hit(&s,&d,label(&d,"Hour"),12));assert(m.draft.civil.hour==12);
 m.has_confirmed[PSM_FORMAT]=true;m.confirmed[PSM_FORMAT].number=1;m.status[PSM_FORMAT]=PSM_UNAVAILABLE;++m.revision;assert(pss_build(&s,&d));assert(!label(&d,"AM")&&label(&d,"Hour")->maximum==23);assert(hit(&s,&d,label(&d,"Hour"),0));assert(m.draft.civil.hour==0);
 m.has_confirmed[PSM_FORMAT]=false;++m.revision;assert(pss_build(&s,&d));assert(label(&d,"AM")&&label(&d,"Hour")->value==12);valid(&d);
}
static void unavailable(void){PortableSettingsModel m;pss_scene s;risc_components_document_v1 d;init(&m,&s);m.store=NULL;m.storage_state=PSM_STORAGE_UNAVAILABLE;assert(psm_refresh(&m));for(unsigned f=0;f<PSM_FEATURE_COUNT;f++){m.route=PSM_ROOT;m.phase=PSM_IDLE;m.clock_active=false;assert(psm_open(&m,(psm_feature)f));assert(pss_build(&s,&d));valid(&d);if(f==PSM_IDLE_TIMER)assert(m.draft.number==m.policy.idle_default_ms);if(f==PSM_DEEP_TIMER)assert(m.draft.number==m.policy.deep_default_ms);}}
int main(void){unavailable();twelve_hour();catalog();routes();fencing();cancellation_and_policy();puts("Portable Settings scene tests passed");return 0;}

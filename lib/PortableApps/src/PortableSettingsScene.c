#include "PortableSettingsScene.h"
#include <string.h>
#include <stdio.h>
#include <limits.h>
/* IDs are semantic and stable across revisions and devices. */
enum { A_SAVE=10,A_CANCEL,A_DISCARD,A_RELOAD,A_REGIONS,A_INITIALS,A_EXIT,
 A_OPEN=100,A_CHOICE=200,A_CIVIL=300,A_PERIOD=350,A_FOLD=400,A_REGION=500,
 A_INITIAL=600,A_ZONE=1000,A_TIMER=2000 };
static const char *last_separator(const char *text){const char *last=NULL;for(;*text;text++)if(*text=='/')last=text;return last;}
static void copy(char *to,size_t n,const char *from){if(n)snprintf(to,n,"%s",from?from:"");}
static uint32_t fingerprint(const PortableSettingsModel *m){
 uint32_t h=m->revision*UINT32_C(16777619)^m->phase^(uint32_t)m->last_result;
 h=h*UINT32_C(16777619)^(uint32_t)m->clock_result.stage;
 h=h*UINT32_C(16777619)^(uint32_t)m->clock_result.rtc_outcome;
 h=h*UINT32_C(16777619)^(uint32_t)m->clock_result.metadata_outcome;
 h=h*UINT32_C(16777619)^(unsigned)m->clock_result.native.attempted^((unsigned)m->clock_result.native.verified<<1);
 for(unsigned i=0;i<sizeof(m->message)&&m->message[i];i++)h=(h^(unsigned char)m->message[i])*UINT32_C(16777619);
 return h;
}
bool pss_needs_publish(const pss_scene *s){return s&&s->model&&(!s->count||s->fingerprint!=fingerprint(s->model));}
static const char *status_text(int s){switch(s){case PSM_LOADED:return "Confirmed";case PSM_MISSING:return "Default, not saved";case PSM_INVALID:return "Invalid stored value";case PSM_UNAVAILABLE:return "Unavailable";case PSM_UNSUPPORTED:return "Unsupported";case PSM_UNCONFIRMED:return "Save unconfirmed";case PSM_CONFLICT:return "Changed elsewhere";default:return "Unknown state";}}
static risc_scene_node_v1 *node(risc_components_document_v1 *d,uint32_t id,uint32_t kind,uint32_t action,const char *label,const char *text){
 if(d->node_count>=RISC_COMPONENTS_MAX_NODES)return NULL;
 unsigned i=d->node_count++;risc_scene_node_v1 *n=&d->nodes[i];
 n->id=id;n->route=d->root;n->kind=kind;n->action=action;if(kind==RISC_COMPONENT_CHECK_ROW){n->maximum=1;n->step=1;}copy(n->label,sizeof(n->label),label);copy(n->text,sizeof(n->text),text);return n;
}
static void action(risc_components_document_v1 *d,unsigned id,const char *label,bool disabled){risc_scene_node_v1 *n=node(d,id,RISC_SCENE_ACTION,id,label,"");if(n&&disabled)n->flags|=RISC_SCENE_DISABLED;}
static void number(risc_components_document_v1 *d,unsigned id,const char *label,int32_t v,int32_t min,int32_t max,int32_t step){risc_scene_node_v1 *n=node(d,id,RISC_COMPONENT_STEPPER,id,label,"");if(n){n->value=v;n->minimum=min;n->maximum=max;n->step=step;snprintf(n->text,sizeof(n->text),"%ld",(long)v);}}
/* Preserve a known confirmed format through temporary unavailability; the
 * established virtual default is 12-hour when no confirmed value exists. */
static bool time_12h(const PortableSettingsModel *m){return !m->has_confirmed[PSM_FORMAT]||m->confirmed[PSM_FORMAT].number!=1;}
static void value_text(psm_feature f,const psm_value *v,char *out,size_t size){
 if(f==PSM_ZONE)copy(out,size,v->zone);
 else if(f==PSM_BASIS)snprintf(out,size,"%s; reference %lu",v->basis.stores_utc?"UTC":"Local calendar",(unsigned long)v->basis.reference_epoch);
 else if(f==PSM_TIME)snprintf(out,size,"%04ld-%02u-%02u %02u:%02u:%02u",(long)v->civil.year,v->civil.month,v->civil.day,v->civil.hour,v->civil.minute,v->civil.second);
 else if(f==PSM_IDLE_TIMER||f==PSM_DEEP_TIMER)snprintf(out,size,"%lu ms",(unsigned long)v->number);
 else {const char *label=psm_choice_label(f,v->number);if(label)copy(out,size,label);else snprintf(out,size,"%lu",(unsigned long)v->number);}
}
void pss_init(pss_scene *s,PortableSettingsModel *m){if(!s)return;memset(s,0,sizeof(*s));s->model=m;s->zone_region=-1;s->zone_initial=-1;}
static int initial(const portable_timezone_entry *e){const char *p=strchr(e->id,'/');p=p?p+1:e->id;unsigned c=(unsigned char)*p;return c>='A'&&c<='Z'?(int)(c-'A'):26;}
static void zone_label(unsigned index,char out[PORTABLE_TIMEZONE_ID_BYTES]){
 const portable_timezone_entry *entry=portable_timezone_get(index);
 char display[PORTABLE_TIMEZONE_ID_BYTES];
 if(portable_timezone_display_name(index,display,sizeof(display))!=PORTABLE_TIMEZONE_OK){copy(out,PORTABLE_TIMEZONE_ID_BYTES,entry->id);return;}
 const char *leaf=last_separator(display);leaf=leaf?leaf+1:display;
 /* Prefer compact city names only while unique inside the logical region.
  * A future catalog collision falls back to the complete regional path. */
 for(unsigned i=0;i<portable_timezone_count();i++)if(i!=index){const portable_timezone_entry *other=portable_timezone_get(i);if(other->region!=entry->region)continue;const char *other_leaf=last_separator(other->id);const char *entry_leaf=last_separator(entry->id);if(!strcmp(other_leaf?other_leaf+1:other->id,entry_leaf?entry_leaf+1:entry->id)){leaf=display;break;}}
 copy(out,PORTABLE_TIMEZONE_ID_BYTES,leaf);
}
static void zones(pss_scene *s,risc_components_document_v1 *d){
 if(s->zone_region<0){for(unsigned r=0;r<PORTABLE_TIMEZONE_REGION_COUNT;r++)action(d,A_REGION+r,portable_timezone_region_name(r),false);return;}
 action(d,A_REGIONS,"All regions",false);
 unsigned count=portable_timezone_region_count((unsigned)s->zone_region);
 /* A region larger than the document vocabulary is a logical alphabetical
  * directory, not a page. This grouping is identical on every provider. */
 if(count>64&&s->zone_initial<0){bool seen[27]={false};for(unsigned i=0;i<portable_timezone_count();i++){const portable_timezone_entry *e=portable_timezone_get(i);if((int)e->region==s->zone_region)seen[initial(e)]=true;}for(unsigned i=0;i<27;i++)if(seen[i]){char label[16];snprintf(label,sizeof(label),"%c",i<26?'A'+i:'#');action(d,A_INITIAL+i,label,false);}return;}
 if(count>64)action(d,A_INITIALS,"All initials",false);
 for(unsigned i=0;i<portable_timezone_count();i++){const portable_timezone_entry *e=portable_timezone_get(i);if((int)e->region!=s->zone_region||(count>64&&initial(e)!=s->zone_initial))continue;
 char label[PORTABLE_TIMEZONE_ID_BYTES];zone_label(i,label);risc_scene_node_v1 *n=node(d,A_ZONE+i,RISC_COMPONENT_CHECK_ROW,A_ZONE+i,label,"");if(n)n->value=!strcmp(s->model->draft.zone,e->id);}
}
bool pss_build(pss_scene *s,risc_components_document_v1 *d){
 if(!s||!s->model||!d||s->revision==UINT32_MAX)return false;
 PortableSettingsModel *m=s->model;if(m->route<PSM_ROOT||m->route>PSM_ABOUT_ROUTE)return false;
 memset(d,0,sizeof(*d));d->api_version=1;d->struct_size=sizeof(*d);d->revision=++s->revision;d->route_count=1;d->root=1;d->routes[0].id=d->root;
 d->screen_key=m->route*100000u+(m->route==PSM_EDITOR?(unsigned)m->editing*1000u+(unsigned)(s->zone_region+1)*32u+(unsigned)(s->zone_initial+1):0);
 copy(d->routes[0].title,sizeof(d->routes[0].title),m->route==PSM_ROOT?"Settings":m->route==PSM_FOLD?"Ambiguous local time":psm_label(m->editing));
 /* A save barrier is meaningful only when its notice is actually visible.
  * Busy state is its own minimal logical screen; fresh screen_key resets any
  * editor scroll, and status/progress/cancel fit the provider's smallest lane. */
 if(m->phase!=PSM_IDLE){
  d->screen_key=UINT32_C(10000000)+m->phase;
  const char *title=m->phase==PSM_RETAINED?"Operation retained":m->phase==PSM_CANCELLING?"Cancelling":"Not confirmed";
  copy(d->routes[0].title,sizeof(d->routes[0].title),title);
  d->routes[0].back_action=A_CANCEL;
  node(d,3006,RISC_SCENE_TEXT_NODE,0,title,m->message);
  node(d,3005,RISC_COMPONENT_PROGRESS,0,"","");
  action(d,A_CANCEL,"Cancel operation",m->phase==PSM_RETAINED);
  goto capture;
 }
 if(m->route==PSM_ROOT){d->flags=RISC_COMPONENTS_ROOT_HEADER;for(unsigned f=0;f<PSM_FEATURE_COUNT;f++)if(m->policy.features&PSM_FEATURE(f)){
 char value[72];if(f==PSM_STORAGE){const char *states[]={"Available","Unavailable","Reload required","Conflict"};copy(value,sizeof(value),m->storage_state<4?states[m->storage_state]:"Unknown");}
 else if(f==PSM_ABOUT)copy(value,sizeof(value),"Portable settings");
 else if(f==PSM_TIME)copy(value,sizeof(value),"Set date and time");
 else if(m->has_confirmed[f]){char confirmed[72];value_text((psm_feature)f,&m->confirmed[f],confirmed,sizeof(confirmed));if(m->status[f]==PSM_LOADED)copy(value,sizeof(value),confirmed);else snprintf(value,sizeof(value),"%.39s; %.28s",confirmed,status_text(m->status[f]));}
 else copy(value,sizeof(value),status_text(m->status[f]));
 risc_scene_node_v1 *n=node(d,A_OPEN+f,RISC_COMPONENT_ROW,A_OPEN+f,psm_label((psm_feature)f),value);if(n&&m->status[f]==PSM_UNSUPPORTED)n->flags|=RISC_SCENE_DISABLED;
 }action(d,A_RELOAD,"Reload",m->phase!=PSM_IDLE);action(d,A_EXIT,"Done",!psm_can_handoff(m));d->routes[0].back_action=A_EXIT;
 }else{
 d->routes[0].back_action=A_CANCEL;
 bool busy=m->phase!=PSM_IDLE;
 if(m->route==PSM_STORAGE_ROUTE){const char *states[]={"Available","Unavailable","Reload required","Conflict"};node(d,3000,RISC_SCENE_TEXT_NODE,0,"Storage",m->storage_state<4?states[m->storage_state]:"Unknown");action(d,A_RELOAD,"Reload",busy);}
 else if(m->route==PSM_ABOUT_ROUTE)node(d,3001,RISC_SCENE_TEXT_NODE,0,"Settings","Portable semantic settings");
 else if(m->route==PSM_FOLD){node(d,3002,RISC_SCENE_TEXT_NODE,0,"Choose an occurrence","This local time occurs twice.");for(unsigned i=0;i<2;i++){risc_scene_node_v1 *n=node(d,A_FOLD+i,RISC_COMPONENT_CHECK_ROW,A_FOLD+i,i?"Later occurrence":"Earlier occurrence","");if(n)n->value=m->fold_choice==(int)i;}}
 else if(m->editing==PSM_ZONE){node(d,3003,RISC_SCENE_TEXT_NODE,0,"Selected time zone",m->draft.zone);if(s->zone_region>=0){copy(d->routes[0].title,sizeof(d->routes[0].title),portable_timezone_region_name((unsigned)s->zone_region));copy(d->subtitle,sizeof(d->subtitle),"Time Zone");}zones(s,d);if(!busy&&s->zone_region>=0)d->routes[0].back_action=s->zone_initial>=0?A_INITIALS:A_REGIONS;}
 else if(m->editing==PSM_BASIS){char basis[72];value_text(PSM_BASIS,&m->draft,basis,sizeof(basis));node(d,3008,RISC_SCENE_TEXT_NODE,0,"RTC interpretation",basis);}
 else if(m->editing==PSM_TIME){portable_timezone_civil *c=&m->draft.civil;number(d,A_CIVIL,"Year",c->year,2000,2099,1);number(d,A_CIVIL+1,"Month",c->month,1,12,1);number(d,A_CIVIL+2,"Day",c->day,1,31,1);bool twelve=time_12h(m);number(d,A_CIVIL+3,"Hour",twelve?(c->hour%12?c->hour%12:12):c->hour,twelve?1:0,twelve?12:23,1);if(twelve){for(unsigned i=0;i<2;i++){risc_scene_node_v1 *period=node(d,A_PERIOD+i,RISC_COMPONENT_CHECK_ROW,A_PERIOD+i,i?"PM":"AM","");if(period)period->value=(c->hour>=12)==(i==1);}}number(d,A_CIVIL+4,"Minute",c->minute,0,59,1);number(d,A_CIVIL+5,"Second",c->second,0,59,1);}
 else if(m->editing==PSM_IDLE_TIMER||m->editing==PSM_DEEP_TIMER)number(d,A_TIMER,"Seconds",(int32_t)(m->draft.number/1000u),m->editing==PSM_DEEP_TIMER?60:5,3600,1);
 else {unsigned count=psm_choice_count(m->editing);
 for(unsigned j=0;j<count;j++){unsigned i=j+(m->editing==PSM_ALERT?1u:0u);if(m->editing==PSM_SLEEP&&!(m->policy.sleep_modes&(1u<<i)))continue;risc_scene_node_v1 *n=node(d,A_CHOICE+i,RISC_COMPONENT_CHECK_ROW,A_CHOICE+i,psm_choice_label(m->editing,i),"");if(n)n->value=m->draft_valid&&m->draft.number==i;}}
 if(m->route==PSM_EDITOR||m->route==PSM_FOLD){node(d,3004,RISC_SCENE_TEXT_NODE,0,"Stored state",status_text(m->status[m->editing]));action(d,A_SAVE,"Save",busy||m->route==PSM_FOLD||!m->draft_valid||!(m->policy.writable&PSM_FEATURE(m->editing))||(m->editing==PSM_SLEEP&&m->policy.desk_lock)||(m->editing==PSM_ALERT&&m->policy.visual_alerts));action(d,A_DISCARD,"Discard draft",busy||m->clock_active);if(m->route==PSM_EDITOR&&m->editing!=PSM_BASIS&&(m->uncertain||m->status[m->editing]==PSM_CONFLICT||m->atomic.submitted))action(d,A_RELOAD,"Reload preconditions",busy);}
 if(m->route==PSM_EDITOR&&(!(m->policy.writable&PSM_FEATURE(m->editing))||(m->editing==PSM_SLEEP&&m->policy.desk_lock)||(m->editing==PSM_ALERT&&m->policy.visual_alerts))){
 for(unsigned i=0;i<d->node_count;i++)if(d->nodes[i].kind==RISC_COMPONENT_CHECK_ROW||d->nodes[i].kind==RISC_COMPONENT_STEPPER)d->nodes[i].flags|=RISC_SCENE_DISABLED;
 }
 action(d,A_CANCEL,busy?"Cancel operation":"Cancel",m->phase==PSM_RETAINED);
 if(busy){for(unsigned i=0;i<d->node_count;i++)if(d->nodes[i].action&&d->nodes[i].action!=A_CANCEL)d->nodes[i].flags|=RISC_SCENE_DISABLED;node(d,3005,RISC_COMPONENT_PROGRESS,0,m->phase==PSM_WAIT_PRESENTATION?"Waiting for visible notice":m->phase==PSM_CANCELLING?"Cancelling":m->phase==PSM_RETAINED?"Operation retained":"Saving","");}
 }
 if(m->route==PSM_ROOT&&m->phase!=PSM_IDLE){for(unsigned i=0;i<d->node_count;i++)if(d->nodes[i].action)d->nodes[i].flags|=RISC_SCENE_DISABLED;node(d,3005,RISC_COMPONENT_PROGRESS,0,m->phase==PSM_RETAINED?"Operation retained":"Operation in progress","");}
 if(m->message[0])node(d,3006,RISC_SCENE_TEXT_NODE,0,m->uncertain?"Outcome unconfirmed":"Status",m->message);
 if(m->editing==PSM_TIME&&(m->clock_result.stage||m->clock_result.rtc_outcome||m->clock_result.native.attempted||m->uncertain)){
 const char *outcomes[]={"Not attempted","Unconfirmed","Confirmed"};
 unsigned rtc=(unsigned)m->clock_result.rtc_outcome,meta=(unsigned)m->clock_result.metadata_outcome;
 node(d,3009,RISC_SCENE_TEXT_NODE,0,"RTC clock",rtc<3?outcomes[rtc]:"Unconfirmed");
 node(d,3010,RISC_SCENE_TEXT_NODE,0,"System clock",m->clock_result.native.verified?"Confirmed":m->clock_result.native.attempted?"Unconfirmed":"Not attempted");
 node(d,3011,RISC_SCENE_TEXT_NODE,0,"RTC metadata",meta<3?outcomes[meta]:"Unconfirmed");
 }
 if(m->apply_unconfirmed)node(d,3007,RISC_SCENE_TEXT_NODE,0,"Saved; apply unconfirmed","The saved preference may not yet be active.");
 capture:
 s->back_action=d->routes[0].back_action;s->count=d->node_count;s->fingerprint=fingerprint(m);s->model_revision=m->revision;s->presented_save_revision=m->phase==PSM_WAIT_PRESENTATION?m->save_revision:0;
 for(unsigned i=0;i<s->count;i++){s->emitted[i].id=d->nodes[i].id;s->emitted[i].action=d->nodes[i].action;s->emitted[i].kind=d->nodes[i].kind;s->emitted[i].flags=d->nodes[i].flags;s->emitted[i].minimum=d->nodes[i].minimum;s->emitted[i].maximum=d->nodes[i].maximum;}
 return true;
}
uint32_t pss_save_revision(const pss_scene *s,uint32_t rev){return s&&s->model&&rev==s->revision&&s->model_revision==s->model->revision&&s->fingerprint==fingerprint(s->model)&&s->model->phase==PSM_WAIT_PRESENTATION&&s->model->save_revision==s->presented_save_revision?s->presented_save_revision:0;}
bool pss_action(pss_scene *s,const risc_scene_event_v1 *e){
 if(!s||!s->model||!e||e->struct_size<sizeof(*e)||e->document_revision!=s->revision||s->model_revision!=s->model->revision||s->fingerprint!=fingerprint(s->model))return false;
 unsigned i;for(i=0;i<s->count;i++)if((s->emitted[i].id==e->node||(!e->node&&e->action==s->back_action))&&s->emitted[i].action==e->action)break;
 if(i==s->count||!e->action||(s->emitted[i].flags&RISC_SCENE_DISABLED))return false;
 bool val=s->emitted[i].kind==RISC_COMPONENT_STEPPER;
 bool check=s->emitted[i].kind==RISC_COMPONENT_CHECK_ROW;
 if(check){if((e->kind!=RISC_SCENE_VALUE_EVENT&&e->kind!=RISC_SCENE_ACTION_EVENT)||e->value<0||e->value>1)return false;}
 else if(e->kind!=(val?RISC_SCENE_VALUE_EVENT:RISC_SCENE_ACTION_EVENT)||(val&&(e->value<s->emitted[i].minimum||e->value>s->emitted[i].maximum)))return false;
 PortableSettingsModel *m=s->model;unsigned a=e->action;bool result=false;
 if(a==A_EXIT){s->exit_requested=psm_can_handoff(m);result=s->exit_requested;}
 else if(a==A_SAVE)result=psm_save(m);
 else if(a==A_CANCEL){psm_cancel(m);s->zone_region=s->zone_initial=-1;result=true;}
 else if(a==A_DISCARD)result=psm_discard(m);
 else if(a==A_RELOAD){if(m->route==PSM_EDITOR)result=psm_reload_draft(m);else {s->reload_requested=true;result=true;}}
 else if(a>=A_OPEN&&a<A_OPEN+PSM_FEATURE_COUNT){s->zone_region=s->zone_initial=-1;result=psm_open(m,(psm_feature)(a-A_OPEN));}
 else if(a>=A_CHOICE&&a<A_CIVIL)result=psm_set_number(m,a-A_CHOICE);
 else if(a>=A_CIVIL&&a<A_CIVIL+6){portable_timezone_civil c=m->draft.civil;switch(a-A_CIVIL){case 0:c.year=e->value;break;case 1:c.month=(uint8_t)e->value;break;case 2:c.day=(uint8_t)e->value;break;case 3:c.hour=(uint8_t)(time_12h(m)?e->value%12+(c.hour>=12?12:0):e->value);break;case 4:c.minute=(uint8_t)e->value;break;case 5:c.second=(uint8_t)e->value;break;}result=psm_set_civil(m,&c);}
 else if(a>=A_PERIOD&&a<A_PERIOD+2){portable_timezone_civil c=m->draft.civil;c.hour=(uint8_t)(c.hour%12+12*(a-A_PERIOD));result=psm_set_civil(m,&c);}
 else if(a>=A_FOLD&&a<A_FOLD+2)result=psm_choose_fold(m,a-A_FOLD);
 else if(a==A_REGIONS){s->zone_region=s->zone_initial=-1;result=true;}
 else if(a==A_INITIALS){s->zone_initial=-1;result=true;}
 else if(a>=A_REGION&&a<A_REGION+PORTABLE_TIMEZONE_REGION_COUNT){s->zone_region=(int)(a-A_REGION);s->zone_initial=-1;result=true;}
 else if(a>=A_INITIAL&&a<A_INITIAL+27){s->zone_initial=(int)(a-A_INITIAL);result=true;}
 else if(a>=A_ZONE&&a<A_ZONE+portable_timezone_count()){const portable_timezone_entry *z=portable_timezone_get(a-A_ZONE);result=psm_set_zone(m,z->id,strlen(z->id)+1);}
 else if(a==A_TIMER)result=psm_set_number(m,(uint32_t)e->value*1000u);
 /* Consume even rejected edits: a second event from this presentation cannot
  * become a valid retry. Build a new revision before accepting more input. */
 s->count=0;s->presented_save_revision=0;return result;
}

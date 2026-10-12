/* Wi-Fi Manager: copied semantic documents and network intents only.
 * Layout, icons, charts, hit maps and keyboard are separately loaded services. */
#include "RiscRuntimeV1.h"
#include "RiscSceneConnectivityV1.h"
#include "RiscSceneResidentV1.h"
#include "RiscTextEntryV1.h"
#include "PortableWifiProfiles.h"
#include "PortableRadioPolicy.h"
#include "WifiApi.h"
#include <string.h>
#include <stdio.h>
#include <limits.h>

enum { MAIN, SAVED, DETAIL, ADD, JOINING, FAILURE, FORGET, NOTICE };
enum { BACK=1, TAB, TOGGLE, SCAN, SELECT_CURRENT, CONNECT, DISCONNECT, PASSWORD,
       ADD_NETWORK, EDIT_SSID, EDIT_PASSWORD, SECURITY, SAVE_CONNECT, CANCEL,
       FORGET_ASK, FORGET_YES, SAVED_PREV, SAVED_NEXT, RETRY, INFORMATION,
       SELECT_SCAN=100, SELECT_SAVED=200 };
enum { DB_NONE, DB_LIST, DB_FIND, DB_SAVE, DB_FORGET };
enum { AFTER_DETAIL, AFTER_CONNECT };
typedef struct {
    const risc_runtime_api_v1 *rt;
    const risc_scene_api_v1 *scene;
    const risc_scene_components_api_v1 *components;
    const risc_scene_telemetry_api_v1 *telemetry;
    const risc_text_entry_api_v1 *text;
    const wifi_async_v1 *radio;
    const risc_key_value_v1 *store,*policy;
    risc_key_value_v1 checked_store,checked_policy;
    const risc_key_value_v1 *raw_store,*raw_policy;
    risc_runtime_capability_v1 grants[5];unsigned acquired;
    risc_scene_resident_v1 resident;
    risc_components_document_v1 doc,next_doc;
    risc_scene_telemetry_v1 plot,last_plot;
    uint64_t session,editor_session,sequence;
    uint32_t now,revision,screen_key,last_view,started_at,connected_at,last_sample,policy_at;
    uint32_t operation,operation_kind,queued_kind,service,shared_reason;
    risc_radio_progress_v1 progress;
    garden_radio_scan_result_v1 scan;
    garden_radio_scan_entry_v1 selected,connected;
    portable_wifi_credentials draft;
    portable_wifi_profile_search search;
    uint32_t db_count,db_next,saved_base,saved_slots[8],selected_slot;
    char saved_names[8][33];unsigned saved_rows,db_job,after_find;
    uint16_t history[60];unsigned history_pos,history_count;
    unsigned screen,parent,editor_field,editor_return,security;
    int8_t connected_rssi;
    uint8_t radio_flags;
    bool retained,exit_requested,finished,upgrade,enabled,connected_known,link_up;
    bool stopping,scanning,dirty,keyboard_pending,editor_done,policy_dirty;
    bool selected_saved,save_on_connect,db_fault,have_scan,selected_live,policy_loading,view_settled,drawing;
    uint32_t editor_result,editor_flags;
    char error[72],message[72],feedback[40];uint32_t toast;
} wifi_manager;
static wifi_manager wm;
static void copy_text(char *to,size_t size,const char *from){size_t n=0;if(from)while(n+1<size&&from[n])++n;memcpy(to,from?from:"",n);to[n]=0;}
static void wipe(void *p,size_t n){volatile unsigned char *b=p;while(n--)*b++=0;}
static bool live(wifi_manager *a){if(a->retained)return false;if(!risc_runtime_get_api(1)){a->retained=true;return false;}return true;}
static bool retain(wifi_manager *a){if(!a->retained&&live(a))a->rt->retain_invocation();a->retained=true;return false;}
/* Keep every callback in the reused profile codec inside the live invocation. */
static int32_t checked_get(void *ctx,const char *key,void *data,uint32_t cap,uint32_t *size){
    const risc_key_value_v1 *raw=ctx;if(!live(&wm)){if(size)*size=0;return RISC_KEY_VALUE_IO;}
    int rc=raw->get(raw->context,key,data,cap,size);return live(&wm)?rc:RISC_KEY_VALUE_IO;
}
static int32_t checked_put(void *ctx,const char *key,const void *data,uint32_t size){
    const risc_key_value_v1 *raw=ctx;if(!live(&wm))return RISC_KEY_VALUE_IO;
    int rc=raw->put(raw->context,key,data,size);return live(&wm)?rc:RISC_KEY_VALUE_IO;
}
static bool scene_result(wifi_manager *a,int rc){return live(a)&&rc==RISC_SCENE_OK?true:retain(a);}
static void route(wifi_manager *a,unsigned page){if(a->screen!=page){a->screen=page;++a->screen_key;}a->dirty=true;}
static void notice(wifi_manager *a,const char *s){copy_text(a->message,sizeof(a->message),s);a->parent=a->screen;route(a,NOTICE);}
static void toast(wifi_manager *a,const char *s){copy_text(a->feedback,sizeof(a->feedback),s);++a->toast;a->dirty=true;}
static unsigned strength(int r){return r>-55?4:r>-65?3:r>-75?2:r>-85?1:0;}
static const char *auth_name(unsigned auth){switch(auth){case 0:return "OPEN";case 1:return "WPA";case 2:return "WPA2";case 3:return "WPA/WPA2";case 4:return "WPA3";case 5:return "WPA2/WPA3";default:return "UNSUPPORTED";}}
static bool api_radio(const wifi_async_v1 *w){return w&&w->base.api_version==1&&w->base.struct_size>=sizeof(*w)&&w->async_tag==RISC_RADIO_ASYNC_TAG&&w->async_version==1&&w->begin&&w->poll&&w->cancel&&w->service_begin&&w->service_end;}
/* Pair storage access with the existing radio provider's PHY-safe lease.
 * Busy is a clean deferral. No app-private storage gate or duplicated codec. */
static bool storage_begin(wifi_manager *a){
    if(!live(a))return false;if(!a->operation)return true;
    uint32_t token=0;int rc=a->radio->service_begin(a->radio->base.context,&token);
    if(!live(a))return false;
    if((rc==RISC_RADIO_AGAIN||rc==RISC_RADIO_BUSY)&&!token)return false;
    if(rc!=RISC_RADIO_ACCEPTED||!token)return retain(a);a->service=token;return true;
}
static bool storage_end(wifi_manager *a){
    if(!live(a))return false;if(!a->service)return true;
    int rc=a->radio->service_end(a->radio->base.context,a->service);if(!live(a)||rc!=RISC_RADIO_ACCEPTED)return retain(a);a->service=0;return true;
}
static void list_start(wifi_manager *a){a->db_job=DB_LIST;a->db_next=0;a->db_count=0;a->saved_rows=0;a->db_fault=false;memset(a->saved_names,0,sizeof(a->saved_names));a->dirty=true;}
static void find_start(wifi_manager *a,unsigned after){
    memset(&a->search,0,sizeof(a->search));a->search.empty=UINT32_MAX;
    copy_text(a->search.ssid,sizeof(a->search.ssid),a->selected.ssid);
    a->db_job=DB_FIND;a->after_find=after;a->selected_saved=false;a->selected_slot=UINT32_MAX;a->db_fault=false;a->dirty=true;
}
static bool stop_radio(wifi_manager *a){
    if(!a->operation)return true;if(a->stopping)return false;
    int rc=a->radio->cancel(a->radio->base.context,a->operation);if(!live(a))return false;
    if(rc!=RISC_RADIO_PENDING&&rc!=RISC_RADIO_AGAIN&&rc!=RISC_RADIO_QUIESCENT)return retain(a);
    a->stopping=true;a->link_up=false;a->scanning=false;a->dirty=true;return false;
}
static void schedule(wifi_manager *a,unsigned kind){
    if(!a->enabled){notice(a,"Turn on Wi-Fi before scanning or joining.");return;}
    if(!a->radio){notice(a,"The asynchronous Wi-Fi provider is unavailable.");return;}
    a->queued_kind=kind;if(a->operation)(void)stop_radio(a);
    if(kind==RISC_RADIO_REQUEST_JOIN){a->save_on_connect=true;route(a,JOINING);}else{a->scanning=true;route(a,MAIN);}
}
static void open_editor(wifi_manager *a,unsigned field){
    if(!a->text||(field&& !risc_text_entry_masked(a->text))){notice(a,"Shared password keyboard unavailable. Update text-input provider.");return;}
    a->editor_field=field;a->editor_return=a->screen;a->keyboard_pending=true;a->dirty=true;
}
static void connect_selected(wifi_manager *a){
    if(a->db_job||a->db_fault){toast(a,"Wait for saved-network lookup");return;}
    if(a->selected.auth==GARDEN_RADIO_AUTH_UNSUPPORTED){notice(a,"Enterprise or unsupported security. This provider cannot join it.");return;}
    if(a->selected.auth==GARDEN_RADIO_AUTH_OPEN){
        if(a->draft.password[0]){notice(a,"Security changed: saved secured network now appears open. Forget it first.");return;}
        schedule(a,RISC_RADIO_REQUEST_JOIN);return;
    }
    if(strlen(a->draft.password)<8)open_editor(a,1);else schedule(a,RISC_RADIO_REQUEST_JOIN);
}
static void db_step(wifi_manager *a){
    if(!a->db_job)return;
    if(!a->store){a->db_job=DB_NONE;a->db_fault=true;toast(a,"Saved-network storage unavailable");return;}
    if(!storage_begin(a))return;unsigned job=a->db_job;int rc=0;bool completed=false;
    portable_wifi_credentials value;memset(&value,0,sizeof(value));
    if(job==DB_LIST){
        if(!a->db_count){rc=portable_wifi_profile_count(a->store,&a->db_count);}
        else if(a->db_next<a->db_count){uint32_t slot=a->db_next;
            rc=portable_wifi_profile_load(a->store,slot,&value);
            if(rc!=PORTABLE_WIFI_PROFILE_AGAIN){a->db_next++;
                if(!rc&&slot>=a->saved_base&&a->saved_rows<8){unsigned i=a->saved_rows++;a->saved_slots[i]=slot;copy_text(a->saved_names[i],33,value.ssid);}
                if(rc==PORTABLE_WIFI_CREDENTIALS_EMPTY)rc=0;
            }
        }
        if(a->db_count&&a->db_next>=a->db_count)completed=true;
    }else if(job==DB_FIND){
        rc=portable_wifi_profile_search_step(a->store,&a->search);
        if(rc!=PORTABLE_WIFI_PROFILE_AGAIN){
            if(!rc||rc==PORTABLE_WIFI_CREDENTIALS_EMPTY){
                a->selected_slot=a->search.index;
                if(!rc){rc=portable_wifi_profile_load(a->store,a->selected_slot,&value);
                    if(!rc){if(a->after_find==AFTER_DETAIL)a->draft=value;a->selected_saved=true;if(a->selected.rssi==-127&&!value.password[0])a->selected.auth=0;}}
                else rc=0;
            }
            if(rc!=PORTABLE_WIFI_PROFILE_AGAIN)completed=true;
        }
    }else if(job==DB_SAVE){rc=portable_wifi_profile_save(a->store,a->selected_slot,&a->draft);completed=rc!=PORTABLE_WIFI_PROFILE_AGAIN;}
    else if(job==DB_FORGET){rc=portable_wifi_profile_forget(a->store,a->selected_slot);if(rc==PORTABLE_WIFI_CREDENTIALS_FORGOTTEN||rc==PORTABLE_WIFI_CREDENTIALS_CLEANUP_REMANENCE)rc=0;completed=rc!=PORTABLE_WIFI_PROFILE_AGAIN;}
    wipe(&value,sizeof(value));if(!live(a)||!storage_end(a))return;
    if(rc==PORTABLE_WIFI_PROFILE_AGAIN)return;
    if(rc){a->db_job=DB_NONE;a->db_fault=true;toast(a,"Storage failed; saved data not replaced");return;}
    if(!completed)return;a->db_job=DB_NONE;a->dirty=true;
    if(job==DB_FIND&&a->after_find==AFTER_CONNECT)connect_selected(a);
    else if(job==DB_SAVE){wipe(a->draft.password,sizeof(a->draft.password));a->selected_saved=true;toast(a,"Network saved");}
    else if(job==DB_FORGET){wipe(&a->draft,sizeof(a->draft));a->selected_saved=false;route(a,SAVED);list_start(a);toast(a,"Network forgotten");}
}
static bool scan_valid(const garden_radio_scan_result_v1 *s){
    if(s->struct_size!=sizeof(*s)||s->count>GARDEN_RADIO_SCAN_MAX||s->state>GARDEN_RADIO_SCAN_FAILED||s->reserved)return false;
    for(unsigned i=0;i<s->count;i++){const garden_radio_scan_entry_v1 *e=&s->entries[i];if(!memchr(e->ssid,0,sizeof(e->ssid))||e->reserved||e->channel>14||(e->auth>5&&e->auth!=255))return false;}return true;
}
static void radio_step(wifi_manager *a){
    if(!a->radio||!live(a))return;
    if(!a->operation&&a->queued_kind){
        if(a->exit_requested||a->shared_reason||!a->enabled){a->queued_kind=0;return;}
        risc_radio_request_v1 req={.struct_size=sizeof(req),.kind=a->queued_kind};
        if(req.kind==RISC_RADIO_REQUEST_JOIN){copy_text(req.ssid,sizeof(req.ssid),a->draft.ssid);copy_text(req.password,sizeof(req.password),a->draft.password);}
        uint32_t token=0;int rc=a->radio->begin(a->radio->base.context,&req,&token);wipe(&req,sizeof(req));
        if(!live(a))return;if(rc==RISC_RADIO_AGAIN&&!token)return;
        if(rc==RISC_RADIO_RETAINED||(rc==RISC_RADIO_ACCEPTED&&!token)||(rc!=RISC_RADIO_ACCEPTED&&token)){retain(a);return;}
        if(rc!=RISC_RADIO_ACCEPTED){a->queued_kind=0;a->scanning=false;notice(a,"Wi-Fi is busy or unavailable. Stop other radio apps and retry.");return;}
        a->operation=token;a->operation_kind=a->queued_kind;a->queued_kind=0;a->started_at=a->now;a->stopping=false;
        memset(&a->progress,0,sizeof(a->progress));a->dirty=true;
    }
    if(!a->operation)return;
    risc_radio_progress_v1 p={.struct_size=sizeof(p)};int rc=a->radio->poll(a->radio->base.context,a->operation,&p);
    if(!live(a))return;if(rc==RISC_RADIO_AGAIN)return;
    if((rc!=RISC_RADIO_PENDING&&rc!=RISC_RADIO_QUIESCENT)||p.struct_size!=sizeof(p)||p.operation_id!=a->operation||p.phase<1||p.phase>RISC_RADIO_IDLE||p.failure>5||p.owned>1||p.quiescent>1||p.station_state>2||p.owned==p.quiescent||((rc==RISC_RADIO_QUIESCENT)!=(p.quiescent!=0))||!scan_valid(&p.scan)||p.reserved[0]||p.reserved[1]||p.reserved[2]||p.reserved[3]||((p.phase==RISC_RADIO_IDLE)!=(p.quiescent!=0))||(p.phase==RISC_RADIO_CONNECTED&&(a->operation_kind!=RISC_RADIO_REQUEST_JOIN||p.station_state!=2))||(p.phase==RISC_RADIO_RESULTS&&a->operation_kind!=RISC_RADIO_REQUEST_SCAN)){retain(a);return;}
    if(p.phase!=a->progress.phase||p.failure!=a->progress.failure)a->dirty=true;
    a->progress=p;
    if(p.quiescent){bool failed=!a->stopping&&p.failure;a->operation=0;a->stopping=false;a->scanning=false;a->link_up=false;a->dirty=true;
        if(failed){copy_text(a->error,sizeof(a->error),"Connection failed. Check password, signal and access point.");route(a,FAILURE);}return;}
    if(a->stopping)return;
    if(p.phase==RISC_RADIO_RESULTS){
        if(a->scanning){a->scan=p.scan;a->have_scan=true;a->scanning=false;
            for(unsigned i=0;i<a->scan.count;i++)for(unsigned j=i+1;j<a->scan.count;j++)if(a->scan.entries[j].rssi>a->scan.entries[i].rssi){garden_radio_scan_entry_v1 t=a->scan.entries[i];a->scan.entries[i]=a->scan.entries[j];a->scan.entries[j]=t;}
            a->dirty=true;}
    }else if(p.phase==RISC_RADIO_CONNECTED){
        if(!a->link_up){a->link_up=true;a->connected_known=true;a->connected=a->selected;a->connected_at=a->now;a->history_count=0;a->history_pos=0;
            if(a->save_on_connect){a->save_on_connect=false;if(a->selected_slot!=UINT32_MAX&&!a->db_fault)a->db_job=DB_SAVE;else toast(a,"Connected; network could not be saved");}route(a,MAIN);}
        a->connected_rssi=p.rssi;
        if((uint32_t)(a->now-a->last_sample)>=1000){a->last_sample=a->now;int r=(int)p.rssi+95;if(r<0)r=0;if(r>50)r=50;a->history[a->history_pos]=(uint16_t)(r*200);a->history_pos=(a->history_pos+1)%60;if(a->history_count<60)a->history_count++;}
    }
    if((a->operation_kind==1&&p.phase!=RISC_RADIO_RESULTS&&(uint32_t)(a->now-a->started_at)>=15000)||(a->operation_kind==2&&p.phase!=RISC_RADIO_CONNECTED&&(uint32_t)(a->now-a->started_at)>=30000)){
        copy_text(a->error,sizeof(a->error),"Timed out. Check signal and password, then retry.");(void)stop_radio(a);route(a,FAILURE);
    }
}
static void select_network(wifi_manager *a,const garden_radio_scan_entry_v1 *entry){
    a->selected=*entry;wipe(&a->draft,sizeof(a->draft));copy_text(a->draft.ssid,sizeof(a->draft.ssid),entry->ssid);
    a->selected_live=a->link_up&&a->connected_known&&!strcmp(entry->ssid,a->connected.ssid);
    a->parent=a->screen;route(a,DETAIL);find_start(a,AFTER_DETAIL);
}
static void action(wifi_manager *a,unsigned intent,int value){
    if(a->upgrade){a->exit_requested=true;return;}
    if(intent>=SELECT_SCAN&&intent<SELECT_SCAN+GARDEN_RADIO_SCAN_MAX){unsigned i=intent-SELECT_SCAN;if(i<a->scan.count)select_network(a,&a->scan.entries[i]);return;}
    if(intent>=SELECT_SAVED&&intent<SELECT_SAVED+8){unsigned i=intent-SELECT_SAVED;if(i<a->saved_rows){garden_radio_scan_entry_v1 e={.auth=GARDEN_RADIO_AUTH_WPA2_PSK,.rssi=-127};copy_text(e.ssid,sizeof(e.ssid),a->saved_names[i]);for(unsigned j=0;j<a->scan.count;j++)if(!strcmp(a->scan.entries[j].ssid,e.ssid)){e=a->scan.entries[j];break;}select_network(a,&e);}return;}
    switch(intent){
    case BACK:if(a->screen==JOINING){a->queued_kind=0;a->save_on_connect=false;(void)stop_radio(a);route(a,MAIN);}else if(a->screen==MAIN||a->screen==SAVED)a->exit_requested=true;else if(a->screen==FORGET)route(a,DETAIL);else route(a,MAIN);break;
    case TAB:route(a,value?SAVED:MAIN);if(value){a->saved_base=0;list_start(a);}break;
    case TOGGLE:a->radio_flags^=PORTABLE_RADIO_WIFI;if(a->radio_flags&PORTABLE_RADIO_WIFI)a->radio_flags&=(uint8_t)~PORTABLE_RADIO_AIRPLANE;a->policy_dirty=true;break;
    case SCAN:schedule(a,RISC_RADIO_REQUEST_SCAN);break;
    case SELECT_CURRENT:if(a->link_up)select_network(a,&a->connected);break;
    case CONNECT:connect_selected(a);break;
    case PASSWORD:open_editor(a,1);break;
    case DISCONNECT:a->queued_kind=0;(void)stop_radio(a);route(a,MAIN);break;
    case CANCEL:a->queued_kind=0;a->save_on_connect=false;(void)stop_radio(a);route(a,MAIN);break;
    case RETRY:connect_selected(a);break;
    case ADD_NETWORK:wipe(&a->draft,sizeof(a->draft));memset(&a->selected,0,sizeof(a->selected));a->selected.auth=2;a->selected.rssi=-127;a->security=1;a->selected_saved=false;a->selected_slot=UINT32_MAX;a->db_job=DB_NONE;route(a,ADD);break;
    case EDIT_SSID:open_editor(a,0);break;
    case EDIT_PASSWORD:open_editor(a,2);break;
    case SECURITY:a->security=(unsigned)value;a->selected.auth=value==0?0:value==2?4:2;if(!value)wipe(a->draft.password,sizeof(a->draft.password));a->dirty=true;break;
    case SAVE_CONNECT:if(portable_wifi_credentials_validate(&a->draft)){copy_text(a->selected.ssid,sizeof(a->selected.ssid),a->draft.ssid);find_start(a,AFTER_CONNECT);}else toast(a,"Name required; password 8 to 63 characters");break;
    case FORGET_ASK:a->parent=a->screen;route(a,FORGET);break;
    case FORGET_YES:if(a->selected_saved&&a->selected_slot!=UINT32_MAX){if(a->selected_live)(void)stop_radio(a);a->db_job=DB_FORGET;}break;
    case SAVED_PREV:a->saved_base=a->saved_base>=8?a->saved_base-8:0;list_start(a);break;
    case SAVED_NEXT:if(a->saved_rows==8){a->saved_base=a->saved_slots[7]+1;list_start(a);}break;
    case INFORMATION:notice(a,"This Wi-Fi provider does not expose static IP, DNS or internet tests.");break;
    default:break;
    }
}
#include "view.inc"
static bool publish(wifi_manager *a){
    declare(a);if(!live(a))return false;
    /* Do not restart a completed static e-ink page with identical data. */
    if(a->session&&!memcmp(&a->next_doc,&a->doc,sizeof(a->doc))&&!memcmp(&a->plot,&a->last_plot,sizeof(a->plot))){a->dirty=false;return true;}
    if(a->revision==UINT32_MAX)return retain(a);
    a->next_doc.revision=++a->revision;
    bool opening=a->session==0;
    int rc=a->session?((a->telemetry&&(a->next_doc.flags&RISC_COMPONENTS_TELEMETRY))?a->telemetry->present(a->scene->context,a->session,&a->next_doc,&a->plot):a->components->update(a->scene->context,a->session,&a->next_doc)):a->components->open(a->scene->context,&a->next_doc,NULL,&a->session);
    if(!scene_result(a,rc))return false;a->doc=a->next_doc;a->doc.revision=0;a->last_plot=a->plot;a->view_settled=false;a->dirty=false;a->last_view=a->now;
    return !opening||scene_result(a,a->components->lifecycle.configure(a->scene->context,a->session,a->resident.enabled?RISC_SCENE_FEATURE_SHARED_CONTROLS:0));
}
static bool close_scene(wifi_manager *a){
    if(!a->session)return true;int rc=a->scene->close(a->scene->context,a->session);if(!live(a))return false;
    if(rc==RISC_SCENE_AGAIN)return false;if(rc!=RISC_SCENE_OK)return retain(a);a->session=0;a->sequence=0;return true;
}
static bool keyboard_step(wifi_manager *a){
    if(a->keyboard_pending){
        if(!close_scene(a))return true;
        risc_text_entry_request_v1 q={.api_version=1,.struct_size=sizeof(q),.capacity=a->editor_field?64:33};
        q.reserved=RISC_TEXT_ENTRY_REQUEST_HOME_REASON|(a->editor_field?RISC_TEXT_ENTRY_REQUEST_MASKED:0);
        copy_text(q.label,sizeof(q.label),a->editor_field?"PASSWORD":"NETWORK NAME");
        copy_text(q.text,sizeof(q.text),a->editor_field?a->draft.password:a->draft.ssid);
        int rc=a->text->open(a->text->context,&q,&a->editor_session);wipe(&q,sizeof(q));if(!live(a))return false;
        if(rc!=RISC_TEXT_ENTRY_OK){if(rc==RISC_TEXT_ENTRY_RETAINED||a->editor_session)return retain(a);a->keyboard_pending=false;notice(a,"Keyboard unavailable; retry when other input has closed.");return publish(a);}
        a->keyboard_pending=false;a->dirty=false;
    }
    if(!a->editor_session)return true;
    if(!a->editor_done){risc_text_entry_state_v1 result={.struct_size=sizeof(result)};
        int rc=a->text->poll(a->text->context,a->editor_session,&result);if(!live(a)){wipe(&result,sizeof(result));return false;}
        if(rc!=RISC_TEXT_ENTRY_OK||result.state>RISC_TEXT_ENTRY_CANCELLED||!memchr(result.text,0,a->editor_field?64:33)){wipe(&result,sizeof(result));return retain(a);}
        if(result.state!=RISC_TEXT_ENTRY_EDITING){a->editor_done=true;a->editor_result=result.state;a->editor_flags=result.flags;
            if(result.state==RISC_TEXT_ENTRY_ACCEPTED)copy_text(a->editor_field?a->draft.password:a->draft.ssid,a->editor_field?64:33,result.text);}
        wipe(&result,sizeof(result));
    }
    if(a->editor_done){int rc=a->text->close(a->text->context,a->editor_session);if(!live(a))return false;if(rc==RISC_TEXT_ENTRY_AGAIN)return true;if(rc!=RISC_TEXT_ENTRY_OK)return retain(a);
        a->editor_session=0;a->editor_done=false;
        if(a->editor_flags&RISC_TEXT_ENTRY_HOME_CANCEL){a->exit_requested=true;return true;}
        route(a,a->editor_return);
        if(a->editor_result==RISC_TEXT_ENTRY_ACCEPTED&&a->editor_field==1){if(strlen(a->draft.password)>=8)schedule(a,RISC_RADIO_REQUEST_JOIN);else toast(a,"Password needs 8 to 63 characters");}
        return publish(a);
    }
    return true;
}
static bool policy_step(wifi_manager *a){
    if(a->policy_loading){
        if(!storage_begin(a))return live(a);
        unsigned loaded=portable_radio_load_result(a->policy,&a->radio_flags);
        if(!live(a)||!storage_end(a))return false;
        if(loaded==PORTABLE_RADIO_BUSY)return true;
        a->policy_loading=false;a->enabled=loaded<=PORTABLE_RADIO_DEFAULT&&(a->radio_flags&PORTABLE_RADIO_WIFI);a->dirty=true;
    }
    if(!a->policy_dirty)return true;if(!a->policy){a->policy_dirty=false;notice(a,"Radio settings storage unavailable");return true;}
    if(!storage_begin(a))return live(a);
    bool ok=portable_radio_save(a->policy,a->radio_flags);if(!live(a)||!storage_end(a))return false;
    a->policy_dirty=false;
    if(!ok){notice(a,"Radio setting was not saved. Reopen Wi-Fi to reload.");return true;}
    a->enabled=(a->radio_flags&PORTABLE_RADIO_WIFI)!=0;
    if(!a->enabled){a->queued_kind=0;(void)stop_radio(a);}else schedule(a,RISC_RADIO_REQUEST_SCAN);
    a->dirty=true;return true;
}
static bool init(wifi_manager *a){
    memset(a,0,sizeof(*a));a->screen_key=1;a->selected_slot=UINT32_MAX;a->selected.rssi=-127;a->dirty=true;
    a->rt=risc_runtime_get_api(1);
    if(!a->rt||a->rt->struct_size<RISC_RUNTIME_MONOTONIC_V1_SIZE||!a->rt->monotonic_ms||!a->rt->acquire||!a->rt->release||!a->rt->retain_invocation||!a->rt->yield_ms)return false;
    const char *caps[]={"ui.scene","ui.text-input","net.wifi","storage.key-value","storage.key-value"};unsigned instances[]={0,0,15,6,1};
    for(unsigned i=0;i<5;i++){risc_runtime_capability_v1 *g=&a->grants[a->acquired];g->struct_size=sizeof(*g);bool found=a->rt->acquire(caps[i],1,instances[i],g);if(!live(a))return false;if(!found){if(!i)return false;continue;}a->acquired++;
        if(i==0){a->scene=g->api;a->components=risc_scene_components_get_v1(a->scene);a->telemetry=risc_scene_telemetry_get_v1(a->scene);
            if(!a->components||!a->components->lifecycle.configure||!a->scene->next||!a->scene->snapshot||!a->scene->close)return false;
            if(!risc_scene_connectivity_get_v1(a->scene)){a->upgrade=true;break;}}
        else if(i==1){const risc_text_entry_api_v1 *t=g->api;if(risc_text_entry_home_reason(t)&&t->open&&t->poll&&t->close)a->text=t;}
        else if(i==2){if(api_radio(g->api))a->radio=g->api;}
        else if(portable_wifi_credentials_api_valid(g->api)){risc_key_value_v1 *checked=i==3?&a->checked_store:&a->checked_policy;*checked=(risc_key_value_v1){1,sizeof(*checked),(void*)g->api,checked_get,checked_put};if(i==3){a->raw_store=g->api;a->store=checked;}else{a->raw_policy=g->api;a->policy=checked;}}
    }
    if(!risc_scene_resident_bind_v1(a->rt,&a->resident))return false;
    if(!a->rt->monotonic_ms(&a->now)||!live(a))return false;
    unsigned loaded=portable_radio_load_result(a->policy,&a->radio_flags);if(!live(a))return false;
    a->enabled=loaded<=PORTABLE_RADIO_DEFAULT&&(a->radio_flags&PORTABLE_RADIO_WIFI);a->policy_loading=loaded==PORTABLE_RADIO_BUSY;
    if(loaded>PORTABLE_RADIO_DEFAULT)copy_text(a->message,sizeof(a->message),"Radio settings unavailable");
    if(a->rt->diagnostic)a->rt->diagnostic("Wi-Fi Manager 2.0.0 scene-only shared-password-keyboard");
    if(!live(a))return false;list_start(a);
    return publish(a);
}
static bool step(wifi_manager *a){
    if(!live(a)||!a->rt->monotonic_ms(&a->now)||!live(a))return false;
    radio_step(a);if(!live(a))return false;
    if(a->exit_requested||a->shared_reason){
        if(a->db_job==DB_SAVE){db_step(a);if(!live(a)||a->db_job==DB_SAVE)return live(a);}
        a->queued_kind=0;if(a->operation){(void)stop_radio(a);return live(a);}
        if(!close_scene(a))return live(a);
        if(a->shared_reason){int rc=risc_scene_resident_dispatch_v1(&a->resident,a->shared_reason);a->shared_reason=0;
            if(!live(a)||rc==RISC_RESIDENT_RETAINED)return retain(a);
            if(rc!=RISC_RESIDENT_OK&&rc!=RISC_RESIDENT_BUSY){a->exit_requested=true;return true;}
            unsigned loaded=portable_radio_load_result(a->policy,&a->radio_flags);if(!live(a))return false;a->enabled=loaded<=PORTABLE_RADIO_DEFAULT&&(a->radio_flags&PORTABLE_RADIO_WIFI);a->policy_loading=loaded==PORTABLE_RADIO_BUSY;a->dirty=true;return publish(a);}
        a->finished=true;return false;
    }
    db_step(a);if(!live(a)||!policy_step(a))return false;
    if(a->keyboard_pending||a->editor_session)return keyboard_step(a);
    if(!a->session&& !publish(a))return false;
    risc_scene_event_v1 e={.struct_size=sizeof(e)};int rc=a->scene->next(a->scene->context,a->session,&e);
    if(!live(a)||(rc!=RISC_SCENE_OK&&rc!=RISC_SCENE_IDLE&&rc!=RISC_SCENE_AGAIN))return retain(a);
    if(rc==RISC_SCENE_OK&&e.document_revision==a->revision&&e.sequence>a->sequence){a->sequence=e.sequence;
        if(e.kind==RISC_SCENE_SUSPEND_EVENT)a->exit_requested=true;
        else if(e.kind==RISC_SCENE_CONTROLS_EVENT&&a->resident.enabled)a->shared_reason=RISC_RESIDENT_CHECKPOINT_CONTROLS;
        else if(!e.node&&e.action==BACK&&e.kind==RISC_SCENE_ACTION_EVENT)action(a,BACK,0);
        else if(e.node&&e.node<=a->doc.node_count){const risc_scene_node_v1 *n=&a->doc.nodes[e.node-1];
            bool values=n->kind==RISC_COMPONENT_SWITCH||n->kind==RISC_COMPONENT_TAB_BAR||n->kind==RISC_COMPONENT_SEGMENTS;
            if(n->action==e.action&&!(n->flags&(RISC_SCENE_DISABLED|RISC_SCENE_HIDDEN))&&((values&&e.kind==2&&e.value>=n->minimum&&e.value<=n->maximum)||(!values&&e.kind==1)))action(a,e.action,e.value);}
    }
    if(a->exit_requested||a->shared_reason||a->keyboard_pending)return true;
    risc_scene_navigation_v1 nav={.struct_size=sizeof(nav)};uint32_t flags=0;
    if(!scene_result(a,a->scene->snapshot(a->scene->context,a->session,&nav,&flags)))return false;
    if((uint32_t)(a->now-a->policy_at)>=100){a->policy_at=a->now;
        int r=risc_scene_resident_poll_work_v1(&a->resident,flags,a->operation||a->db_job?RISC_RESIDENT_POLL_INHIBIT_IDLE|RISC_RESIDENT_POLL_INHIBIT_POLICY:0);
        if(r==RISC_RESIDENT_RETAINED||!live(a))return retain(a);
        if(r==RISC_RESIDENT_EXIT)a->exit_requested=true;
        if(a->resident.policy&&!a->operation&&!a->db_job)a->shared_reason=RISC_RESIDENT_CHECKPOINT_POLICY;
    }
    a->drawing=(flags&RISC_SCENE_PRESENTING)!=0;
    if(!a->drawing&&!a->view_settled){a->view_settled=true;a->last_view=a->now;}
    if(!(flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY))&&(a->dirty||((a->link_up||a->scanning||a->screen==JOINING)&&(uint32_t)(a->now-a->last_view)>=2000)))return publish(a);
    return true;
}
static void cleanup(wifi_manager *a){
    if(!live(a))return;
    if(a->operation||a->session||a->editor_session){retain(a);return;}
    wipe(&a->draft,sizeof(a->draft));
    while(a->acquired){if(!live(a))return;bool ok=a->rt->release(&a->grants[a->acquired-1]);if(!live(a)||!ok){retain(a);return;}a->acquired--;}
}
__attribute__((visibility("default"))) const risc_resident_app_descriptor_v1_t risc_resident_app_descriptor_v1={1,sizeof(risc_resident_app_descriptor_v1_t),RISC_RESIDENT_ROLE_FOREGROUND,0};
__attribute__((visibility("default"))) void app_main(void){wifi_manager *a=&wm;if(init(a)){while(step(a))a->rt->yield_ms(a->drawing||a->editor_session?1:10);}cleanup(a);}
#ifdef WIFI_MANAGER_TEST
bool wifi_manager_test_init(void){return init(&wm);}
bool wifi_manager_test_step(void){return step(&wm);}
void wifi_manager_test_action(unsigned id,int v){action(&wm,id,v);}
const wifi_manager *wifi_manager_test_state(void){return &wm;}
#endif

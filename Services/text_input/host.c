/* The sole plain-text session owner. Apps exchange copied requests/results.
 * Rendering and touch/navigation stay in the separately loaded scene host. */
#include "RiscTextEntryV1.h"
#include "RiscRuntimeV1.h"
#include "RiscSceneV1.h"
#include "SceneKeyboardV1.h"
#include "RiscProviderV2.h"
#include "RiscUsbHidV1.h"
#include <limits.h>
#include <stdbool.h>
#include <string.h>

static const risc_scene_api_v1 *scene;
static const risc_usb_keyboard_api_v1 *keyboard;
static risc_scene_document_v1 document;
static risc_text_entry_state_v1 state;
static uint64_t serial,session,scene_session,subscription,last_sequence;
static uint32_t capacity,page;
static bool started,active,closing,retained,connected,neutral,caps,changed,draining;

static int32_t retain(void){retained=true;return RISC_TEXT_ENTRY_RETAINED;}
static bool live(void){if(retained)return false;if(!risc_runtime_get_api(1)){retain();return false;}return true;}
static int32_t check(uint64_t s){
    if(retained)return RISC_TEXT_ENTRY_RETAINED;
    if(!started||!active||!s||s!=session)return RISC_TEXT_ENTRY_STALE;
    return live()?RISC_TEXT_ENTRY_OK:RISC_TEXT_ENTRY_RETAINED;
}
static bool ascii(const char *s,size_t bytes){
    for(size_t i=0;i<bytes;i++){unsigned char c=(unsigned char)s[i];if(!c)return true;if(c<32||c>126)return false;}
    return false;
}
static bool changed_state(void){
    if(state.revision==UINT32_MAX){retain();return false;}
    ++state.revision;changed=true;return true;
}
static unsigned character(uint8_t key,uint8_t mods){
    bool shift=(mods&0x22u)!=0;
    if(mods&0xddu)return 0;
    if(key>=4&&key<=29)return (shift!=caps?'A':'a')+key-4;
    if(key>=30&&key<=39){static const char plain[]="1234567890",upper[]="!@#$%^&*()";if(shift)return (unsigned char)upper[key-30];return (unsigned char)plain[key-30];}
    switch(key){
    case 0x28:return RISC_SCENE_KEY_DONE;case 0x29:return RISC_SCENE_KEY_CANCEL;case 0x2a:return RISC_SCENE_KEY_BACKSPACE;
    case 0x2c:return ' ';case 0x2d:return shift?'_':'-';case 0x2e:return shift?'+':'=';
    case 0x2f:return shift?'{':'[';case 0x30:return shift?'}':']';case 0x31:return shift?'|':'\\';
    case 0x33:return shift?':':';';case 0x34:return shift?'"':'\'';case 0x35:return shift?'~':'`';
    case 0x36:return shift?'<':',';case 0x37:return shift?'>':'.';case 0x38:return shift?'?':'/';default:return 0;
    }
}
static void apply(unsigned key){
    if(state.state!=RISC_TEXT_ENTRY_EDITING)return;
    if(key==RISC_SCENE_KEY_DONE){state.state=RISC_TEXT_ENTRY_ACCEPTED;(void)changed_state();return;}
    if(key==RISC_SCENE_KEY_CANCEL){state.state=RISC_TEXT_ENTRY_CANCELLED;(void)changed_state();return;}
    if(key==RISC_SCENE_KEY_LAYER){page=(page+1)%4;(void)changed_state();return;}
    size_t n=strlen(state.text);
    if(key==RISC_SCENE_KEY_BACKSPACE){if(n){state.text[n-1]=0;(void)changed_state();}return;}
    if(key==RISC_SCENE_KEY_SPACE)key=' ';
    if(key>=32&&key<=126&&n+1<capacity){state.text[n]=(char)key;state.text[n+1]=0;(void)changed_state();}
}
static bool snapshot(bool reset,bool *held){
    risc_usb_keyboard_state_v1 items[RISC_USB_HID_MAX_INTERFACES]={{0}};
    size_t count=RISC_USB_HID_MAX_INTERFACES;
    bool ok=keyboard->snapshot(keyboard->context,items,&count);
    if(!live()||!ok||count>RISC_USB_HID_MAX_INTERFACES)return false;
    bool attached=false;*held=false;
    for(size_t i=0;i<count;i++){
        if(items[i].connected>1)return false;
        if(!items[i].connected)continue;
        if(!items[i].device)return false;
        for(size_t j=0;j<i;j++)if(items[j].connected&&items[j].device==items[i].device)return false;
        attached=true;*held|=items[i].modifiers!=0;
        for(unsigned k=0;k<6;k++)*held|=items[i].keys[k]!=0;
    }
    if(attached!=connected){connected=attached;state.flags=attached?RISC_TEXT_ENTRY_HARDWARE:0;if(!changed_state())return false;reset=true;}
    if(reset){neutral=!*held;caps=false;}
    return true;
}
static int32_t keyboard_step(void){
    if(!keyboard)return RISC_TEXT_ENTRY_OK;
    bool was_neutral=neutral,discard=draining,held=false;
    bool polled=keyboard->poll(keyboard->context,8);discard|=!polled;
    if(!live())return RISC_TEXT_ENTRY_RETAINED;
    risc_usb_keyboard_event_v1 events[RISC_USB_INPUT_QUEUE_LENGTH];unsigned count=0;
    for(;count<RISC_USB_INPUT_QUEUE_LENGTH;count++){
        risc_usb_keyboard_event_v1 e={0};int32_t r=keyboard->next(keyboard->context,subscription,&e);
        if(!live())return RISC_TEXT_ENTRY_RETAINED;
        if(r==-2)return retain();
        if(r==0)break;
        if(r==-1||e.kind==5){discard=true;continue;}
        if(r!=1||e.kind<1||e.kind>4||e.reserved||!e.sequence||e.sequence<=last_sequence)return retain();
        last_sequence=e.sequence;events[count]=e;
        if(e.kind==1||e.kind==2)discard=true;
    }
    draining=count==RISC_USB_INPUT_QUEUE_LENGTH;
    if(draining)discard=true;
    bool before=connected;
    if(!snapshot(discard||!was_neutral,&held))return retain();
    if(before!=connected)discard=true;
    if(discard||!was_neutral){neutral=!held;return RISC_TEXT_ENTRY_OK;}
    for(unsigned i=0;i<count&&state.state==RISC_TEXT_ENTRY_EDITING;i++){
        const risc_usb_keyboard_event_v1 *e=&events[i];
        if(!connected||e->kind!=3)continue;
        if(e->usage==0x39){if(!(e->modifiers&0xddu))caps=!caps;continue;}
        apply(character(e->usage,e->modifiers));if(retained)return RISC_TEXT_ENTRY_RETAINED;
    }
    return RISC_TEXT_ENTRY_OK;
}
static int32_t update_scene(void){
    document.revision=state.revision;
    document.nodes[0].value=(int32_t)page;
    document.nodes[0].flags=connected?RISC_SCENE_DISABLED:0;
    memcpy(document.nodes[0].text,state.text,sizeof(state.text));
    int32_t r=scene->update(scene->context,scene_session,&document);
    if(!live())return RISC_TEXT_ENTRY_RETAINED;
    if(r!=RISC_SCENE_OK)return retain();
    changed=false;return RISC_TEXT_ENTRY_OK;
}
static int32_t close_owned(void){
    closing=true;
    if(subscription){
        bool ok=keyboard->unsubscribe(keyboard->context,subscription);
        if(!live())return RISC_TEXT_ENTRY_RETAINED;
        if(!ok)return retain();
        subscription=0;
    }
    if(scene_session){
        int32_t r=scene->close(scene->context,scene_session);
        if(!live())return RISC_TEXT_ENTRY_RETAINED;
        if(r==RISC_SCENE_AGAIN)return RISC_TEXT_ENTRY_AGAIN;
        if(r!=RISC_SCENE_OK)return retain();
        scene_session=0;
    }
    active=closing=connected=neutral=caps=changed=draining=false;session=0;
    memset(&state,0,sizeof(state));memset(&document,0,sizeof(document));return RISC_TEXT_ENTRY_OK;
}
static int32_t open_session(void *c,const risc_text_entry_request_v1 *request,uint64_t *out){
    (void)c;if(!live())return RISC_TEXT_ENTRY_RETAINED;
    if(!started||!out||!request||request->api_version!=1||request->struct_size!=sizeof(*request)||
       request->reserved||!request->capacity||request->capacity>RISC_TEXT_ENTRY_BYTES||
       !ascii(request->text,request->capacity)||!ascii(request->label,sizeof(request->label)))return RISC_TEXT_ENTRY_INVALID;
    if(active)return RISC_TEXT_ENTRY_BUSY;
    if(serial==UINT64_MAX)return RISC_TEXT_ENTRY_UNAVAILABLE;
    state=(risc_text_entry_state_v1){.struct_size=sizeof(state),.revision=1};
    memcpy(state.text,request->text,strlen(request->text)+1);
    capacity=request->capacity;page=0;last_sequence=0;connected=caps=closing=changed=draining=false;neutral=true;
    active=true;session=++serial;
    if(keyboard){
        subscription=keyboard->subscribe(keyboard->context,0);
        if(!live())return RISC_TEXT_ENTRY_RETAINED;
        if(!subscription){(void)close_owned();return retained?RISC_TEXT_ENTRY_RETAINED:RISC_TEXT_ENTRY_UNAVAILABLE;}
        bool held=false;if(!snapshot(true,&held)){if(retained)return RISC_TEXT_ENTRY_RETAINED;(void)close_owned();return retained?RISC_TEXT_ENTRY_RETAINED:RISC_TEXT_ENTRY_UNAVAILABLE;}
    }
    document=(risc_scene_document_v1){.api_version=1,.struct_size=sizeof(document),.revision=state.revision,
        .root=1,.route_count=1,.node_count=1,.routes={{.id=1,.back_action=2}},
        .nodes={{.id=1,.route=1,.kind=RISC_SCENE_KEYBOARD_NODE,.action=1,.minimum=0,.maximum=3,.step=1}}};
    memcpy(document.routes[0].title,request->label,sizeof(request->label));
    memcpy(document.nodes[0].label,request->label,sizeof(request->label));
    memcpy(document.nodes[0].text,state.text,sizeof(state.text));
    document.nodes[0].flags=connected?RISC_SCENE_DISABLED:0;
    int32_t r=scene->open(scene->context,&document,NULL,&scene_session);
    if(!live())return RISC_TEXT_ENTRY_RETAINED;
    if(r==RISC_SCENE_RETAINED)return retain();
    if(r!=RISC_SCENE_OK){if(scene_session)return retain();int32_t closed=close_owned();return closed==RISC_TEXT_ENTRY_OK?RISC_TEXT_ENTRY_UNAVAILABLE:closed;}
    if(!scene_session)return retain();
    changed=false;*out=session;return RISC_TEXT_ENTRY_OK;
}
static int32_t poll_session(void *c,uint64_t s,risc_text_entry_state_v1 *out){
    (void)c;int32_t r=check(s);if(r)return r;
    if(!out||out->struct_size!=sizeof(*out))return RISC_TEXT_ENTRY_INVALID;
    if(closing)return RISC_TEXT_ENTRY_BUSY;
    if(state.state==RISC_TEXT_ENTRY_EDITING){
        r=keyboard_step();if(r)return r;
        if(changed){r=update_scene();if(r)return r;}
        risc_scene_event_v1 event={.struct_size=sizeof(event)};
        r=scene->next(scene->context,scene_session,&event);
        if(!live())return RISC_TEXT_ENTRY_RETAINED;
        if(r==RISC_SCENE_OK){
            if(event.document_revision==document.revision){
                if(event.kind==RISC_SCENE_SUSPEND_EVENT||(event.kind==RISC_SCENE_ACTION_EVENT&&event.action==2))apply(RISC_SCENE_KEY_CANCEL);
                else if(event.kind==RISC_SCENE_VALUE_EVENT&&event.node==1&&event.action==1&&
                    (!connected||event.value==RISC_SCENE_KEY_DONE||event.value==RISC_SCENE_KEY_CANCEL)){
                    apply((unsigned)event.value);if(!changed&&!changed_state())return RISC_TEXT_ENTRY_RETAINED;
                }
            }
        }else if(r!=RISC_SCENE_IDLE&&r!=RISC_SCENE_AGAIN)return retain();
        if(retained)return RISC_TEXT_ENTRY_RETAINED;
        if(changed){r=update_scene();if(r)return r;}
    }
    risc_scene_navigation_v1 path={.struct_size=sizeof(path)};uint32_t flags=0;
    r=scene->snapshot(scene->context,scene_session,&path,&flags);
    if(!live())return RISC_TEXT_ENTRY_RETAINED;
    if(r!=RISC_SCENE_OK)return retain();
    *out=state;if(flags&RISC_SCENE_PRESENTING)out->flags|=RISC_TEXT_ENTRY_PRESENTING;return RISC_TEXT_ENTRY_OK;
}
static int32_t close_session(void *c,uint64_t s){(void)c;int32_t r=check(s);return r?r:close_owned();}
static const risc_text_entry_api_v1 api={1,sizeof(api),NULL,open_session,poll_session,close_session};
static bool start(const risc_provider_dependency_v1 *deps,size_t count){
    if(started||active||retained||!deps||!count||count>2)return false;
    scene=NULL;keyboard=NULL;
    for(size_t i=0;i<count;i++){
        if(!deps[i].capability_id||deps[i].api_version!=1||!deps[i].api)return false;
        if(!strcmp(deps[i].capability_id,RISC_SCENE_CAPABILITY)){if(scene)return false;scene=deps[i].api;}
        else if(!strcmp(deps[i].capability_id,"usb.hid.keyboard")){if(keyboard)return false;keyboard=deps[i].api;}
        else return false;
    }
    if(!scene||scene->api_version!=1||scene->struct_size<sizeof(*scene)||!scene->open||!scene->next||!scene->update||!scene->snapshot||!scene->close)return false;
    if(keyboard&&(keyboard->api_version!=1||keyboard->struct_size<sizeof(*keyboard)||!keyboard->subscribe||
        !keyboard->unsubscribe||!keyboard->poll||!keyboard->next||!keyboard->snapshot))return false;
    started=true;return true;
}
static bool quiesce(void){return !retained&&!active&&!scene_session&&!subscription;}
static void stop(void){if(!quiesce())return;started=false;scene=NULL;keyboard=NULL;}
static const risc_driver_v2 driver={2,sizeof(driver),"text-input-host",RISC_TEXT_ENTRY_CAPABILITY,1,&api,start,stop,quiesce};
__attribute__((visibility("default"))) const risc_driver_v2 *t5_driver_get(uint32_t abi){return abi==2?&driver:NULL;}

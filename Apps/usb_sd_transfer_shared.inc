/* Shared semantic presentation: no display geometry or product selector.
 * The existing MSC controller above remains the sole media-custody owner. */
#include "RiscSceneComponentsV1.h"
static risc_runtime_capability_v1 scene_grant;
static const risc_scene_api_v1 *scene;
static const risc_scene_components_api_v1 *components;
static uint64_t scene_session,event_sequence;
static uint32_t document_revision,painted_state;
static bool shared_terminal,scene_owned,closing,controls_requested;
static bool msc_available;
static const char *destination="default.elf";
static risc_components_document_v1 document;
enum { USB_START_STOP=1,USB_BACK=2,USB_REMOVED=3,USB_KEEP=4,USB_ALARM_DISMISS=5,USB_ALARM_RETRY=6,USB_CONTROLS=7 };
static bool shared_live(void) {
    return !shared_terminal && risc_runtime_get_api(1)==runtime;
}
static void shared_retain(void) {
    if(shared_terminal)return;
    shared_terminal=true;
    if(risc_runtime_get_api(1)==runtime &&
       runtime->struct_size>=RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE &&
       runtime->retain_invocation)(void)runtime->retain_invocation();
}
/* Tagged alarm service remains optional. Its schedule/data/output policy stays
 * in the provider. Calls occur only at a settled scene boundary; USB's owner
 * polling continues while an alarm is visible, as in the original application. */
#include "AlarmServiceV2.h"
#include "RiscResidentShellV1.h"
__attribute__((visibility("default")))
const risc_resident_app_descriptor_v1_t risc_resident_app_descriptor_v1={
 1,sizeof(risc_resident_app_descriptor_v1_t),RISC_RESIDENT_ROLE_FOREGROUND,0};
static risc_resident_client_v1 resident;
static risc_runtime_capability_v1 alarm_grant;
static const alarm_service_v1 *alarm_api;
static alarm_status_v1 alarm_status;
static bool alarm_modal,alarm_pending;
static bool alarm_result(int32_t rc) {
 if(!shared_live())return false;
 if(rc==ALARM_RETAINED||rc==ALARM_OUTPUT||rc<ALARM_FOREGROUND||rc>ALARM_PENDING){
  shared_retain();return false;
 }
 return true;
}
static bool alarm_open(void) {
 alarm_grant=(risc_runtime_capability_v1){.struct_size=sizeof(alarm_grant)};
 bool got=runtime->acquire(ALARM_SERVICE_CAPABILITY,2,0,&alarm_grant);
 if(!shared_live())return false;
 if(!got){
  if(alarm_grant.api||alarm_grant.slot||alarm_grant.generation)shared_retain();
  return shared_live();
 }
 alarm_api=alarm_grant.api;
 if(!alarm_service_descriptor(alarm_api)){shared_retain();return false;}
 return true;
}
static bool alarm_pump(void) {
 if(!alarm_api)return true;
 int32_t rc=alarm_api->step(alarm_api->context);
 if(!alarm_result(rc))return false;
 alarm_pending=rc==ALARM_PENDING;
 alarm_status_v1 next={.struct_size=sizeof(next)};
 rc=alarm_api->status(alarm_api->context,&next);
 if(!alarm_result(rc))return false;
 if(rc!=ALARM_OK||next.api_version!=1||next.struct_size<sizeof(next)||
    next.state>ALARM_STATE_CUE||!alarm_result(next.error)||next.output_uncertain){
  shared_retain();return false;
 }
 next.label[sizeof(next.label)-1]=0;
 bool modal=next.state!=ALARM_STATE_READY&&next.state!=ALARM_STATE_CUE;
 if(modal!=alarm_modal||next.state!=alarm_status.state||
    next.snapshot!=alarm_status.snapshot||next.error!=alarm_status.error||
    strcmp(next.label,alarm_status.label))dirty=true;
 alarm_status=next;alarm_modal=modal;
 return true;
}
static bool alarm_owned(void) {
 return alarm_api && (alarm_pending||alarm_status.state==ALARM_STATE_CUE||
  alarm_status.occurrence.generation||alarm_status.output_uncertain||
  alarm_status.state==ALARM_STATE_ALERT||alarm_status.state==ALARM_STATE_DISMISSING);
}
static bool alarm_close(void) {
 if(!alarm_api)return true;
 if(alarm_owned()||alarm_modal)return false;
 if(!runtime->release(&alarm_grant)||!shared_live()){shared_retain();return false;}
 alarm_api=NULL;return true;
}
static bool resident_bind(void) {
 if(runtime->struct_size<RISC_RUNTIME_RESIDENT_SHELL_V1_SIZE||!runtime->resident_shell)return true;
 resident.struct_size=sizeof(resident);
 bool got=runtime->resident_shell(&resident);
 if(!shared_live())return false;
 if(!got){resident=(risc_resident_client_v1){0};return true;}
 if(resident.api_version!=1||resident.struct_size<sizeof(resident)||
    !resident.invocation||resident.role!=RISC_RESIDENT_ROLE_FOREGROUND||
    !resident.checkpoint){shared_retain();return false;}
 return true;
}

static bool shared_result(int32_t rc) {
    if(!shared_live() || rc==RISC_SCENE_RETAINED){shared_retain();return false;}
    return rc==RISC_SCENE_OK;
}
static bool shared_probe_msc(void) {
    risc_runtime_capability_v1 probe={.struct_size=sizeof(probe)};
    bool got=runtime->acquire(RISC_USB_DEVICE_MSC_CAPABILITY,1,0,&probe);
    if(!shared_live())return false;
    if(!got) {
        if(probe.api||probe.slot||probe.generation){shared_retain();return false;}
        msc_available=false;
    } else {
        const risc_usb_device_msc_api_v1 *api=probe.api;
        msc_available=api && api->api_version==1 && api->struct_size>=sizeof(*api) &&
          api->begin && api->poll && api->end && api->last_error &&
          risc_usb_device_msc_prepare(api);
        if(!runtime->release(&probe)||!shared_live()){shared_retain();return false;}
    }
    if(!msc_available) {
        state=RISC_USB_MSC_MEDIA_UNAVAILABLE;
        snprintf(detail,sizeof(detail),"USB SD transfer is unavailable: no compatible authorized hardware provider.");
    }
    return true;
}

static void shared_node(uint32_t id,uint32_t kind,const char *label,
                        const char *text,uint32_t action,uint32_t flags) {
    unsigned i=document.node_count++;
    risc_scene_node_v1 *n=&document.nodes[i];
    n->id=id;n->route=1;n->kind=kind;n->action=action;n->flags=flags;
    snprintf(n->label,sizeof(n->label),"%s",label);
    snprintf(n->text,sizeof(n->text),"%s",text);
}
static void shared_document(void) {
    memset(&document,0,sizeof(document));
    document.api_version=1;document.struct_size=sizeof(document);
    document.revision=++document_revision;document.root=1;document.route_count=1;
    document.routes[0].id=1;
    document.routes[0].back_action=alarm_modal?0:confirm_removed?USB_KEEP:USB_BACK;
    snprintf(document.routes[0].title,sizeof(document.routes[0].title),"USB SD TRANSFER");
    document.screen_key=confirm_removed?2:1;
    shared_node(10,RISC_COMPONENT_SECTION,status_name(),"",0,0);
    if(alarm_modal) {
        document.screen_key=3;document.flags=RISC_COMPONENTS_CONFIRM;
        shared_node(11,RISC_SCENE_TEXT_NODE,"ALARM",alarm_status.label,0,0);
        const char *message=alarm_status.state==ALARM_STATE_LOADING?"Loading saved alarms.":
          alarm_status.state==ALARM_STATE_DISMISSING?"Dismissing alarm.":
          alarm_status.state==ALARM_STATE_BLOCKED?"Alarm service needs attention. Retry.":"Alarm is active.";
        shared_node(12,RISC_SCENE_TEXT_NODE,"",message,0,0);
        if(alarm_status.state==ALARM_STATE_ALERT)
          shared_node(20,RISC_SCENE_ACTION,"DISMISS","",USB_ALARM_DISMISS,RISC_SCENE_PRIMARY);
        if(alarm_status.state==ALARM_STATE_BLOCKED)
          shared_node(20,RISC_SCENE_ACTION,"RETRY","",USB_ALARM_RETRY,RISC_SCENE_PRIMARY);
        if(portable_usb_transfer_owned())
          shared_node(22,RISC_SCENE_TEXT_NODE,"USB transfer continues","Keep the cable connected until safely ejected.",0,0);
    } else if(confirm_removed) {
        document.flags=RISC_COMPONENTS_CONFIRM;
        document.cancel_action=USB_KEEP;
        shared_node(11,RISC_SCENE_TEXT_NODE,"Eject on your computer",
                    "If eject is unavailable, stop all copies and unplug the USB cable.",0,0);
        shared_node(12,RISC_SCENE_TEXT_NODE,"Data-loss warning",
                    "Unplugging during a write can lose files.",0,0);
        shared_node(20,RISC_SCENE_ACTION,"CABLE REMOVED","",USB_REMOVED,
                    RISC_SCENE_DESTRUCTIVE);
        shared_node(21,RISC_SCENE_ACTION,"KEEP TRANSFERRING","",USB_KEEP,0);
    } else {
        const char *message=!msc_available?"USB SD transfer is not available here. Back returns to apps.":state==RISC_USB_MSC_PREPARING?
            "Finishing log writes. Keep this screen open or cancel.":
            unknown_custody?"Ownership is uncertain. Restart required.":
            state==RISC_USB_MSC_FAULT_RETAINED?
            "SD remains reserved. Retry Stop. If it fails, restart the device.":
            state==RISC_USB_MSC_SUSPENDED?
            "SD is still shared. Wake your computer. Eject after copying.":
            portable_usb_transfer_owned()?
            "Keep this screen open while copying. Safely eject when finished.":
            terminal(state)?(local_ready?"SD is available locally.":
            "USB ownership released. Local media readiness is not confirmed."):
            "Connect a USB data cable. Start shares the SD card.";
        shared_node(11,RISC_SCENE_TEXT_NODE,"",message,0,0);
        shared_node(12,RISC_SCENE_TEXT_NODE,"Safe removal",
                    "Eject the drive on your computer after all copies finish.",0,0);
        if(state==RISC_USB_MSC_WAITING)
            shared_node(13,RISC_SCENE_TEXT_NODE,"Waiting",
                        "Connect the USB data cable to a computer.",0,0);
        shared_node(20,RISC_SCENE_ACTION,!msc_available?"USB TRANSFER UNAVAILABLE":state==RISC_USB_MSC_PREPARING?
                    "CANCEL PREPARATION":portable_usb_transfer_owned()?
                    "STOP TRANSFER":"START TRANSFER","",USB_START_STOP,RISC_SCENE_PRIMARY|(!msc_available?RISC_SCENE_DISABLED:0));
        shared_node(21,RISC_SCENE_ACTION,portable_usb_transfer_owned()?
                    "HOME LOCKED DURING TRANSFER":"BACK TO APPS","",USB_BACK,
                    portable_usb_transfer_owned()?RISC_SCENE_DISABLED:0);
    }
    if(resident.invocation&&!alarm_modal&&!confirm_removed)
        shared_node(23,RISC_SCENE_ACTION,"QUICK ACTIONS","",USB_CONTROLS,
          portable_usb_transfer_owned()||alarm_owned()?RISC_SCENE_DISABLED:0);
    /* Preserve the entire bounded provider reason, not only its first row. */
    if(detail[0]) {
        char first[RISC_SCENE_TEXT],second[RISC_SCENE_TEXT];
        size_t n=strlen(detail),take=n<sizeof(first)-1?n:sizeof(first)-1;
        memcpy(first,detail,take);first[take]=0;
        snprintf(second,sizeof(second),"%s",detail+take);
        shared_node(30,RISC_SCENE_TEXT_NODE,"Provider",first,0,0);
        if(second[0])shared_node(31,RISC_SCENE_TEXT_NODE,"",second,0,0);
    }
}
static bool shared_event_valid(const risc_scene_event_v1 *e) {
 if(e->node==0)return e->action && e->action==document.routes[0].back_action;
 for(unsigned i=0;i<document.node_count;i++) {
  const risc_scene_node_v1 *n=&document.nodes[i];
  if(n->id==e->node)return n->kind==RISC_SCENE_ACTION && n->action==e->action &&
      !(n->flags&RISC_SCENE_DISABLED);
 }
 return false;
}
static bool shared_close(void) {
    if(!shared_live())return false;
    if(scene_session) {
        int32_t rc=scene->close(scene->context,scene_session);
        if(!shared_live() || rc==RISC_SCENE_RETAINED){shared_retain();return false;}
        if(rc==RISC_SCENE_AGAIN)return false;
        if(rc!=RISC_SCENE_OK){shared_retain();return false;}
        scene_session=0;
    }
    if(scene_owned) {
        if(!runtime->release(&scene_grant)||!shared_live()){shared_retain();return false;}
        scene_owned=false;
    }
    return true;
}
__attribute__((visibility("default"))) int app_module_init(void) {
    runtime=risc_runtime_get_api(1);
    return !runtime || runtime->api_version!=1 ||
        runtime->struct_size<RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE ||
        !runtime->acquire || !runtime->release || !runtime->yield_ms ||
        !runtime->request_launch || !runtime->retain_invocation ? -1:0;
}
__attribute__((visibility("default"))) void app_module_fini(void) {
    if(!shared_live())return;
    if(portable_usb_transfer_owned()||scene_session||scene_owned||alarm_api)shared_retain();
}
__attribute__((visibility("default"))) void app_main(void) {
    if(!shared_live())return;
    grant=(risc_runtime_capability_v1){0};usb=NULL;preparation=NULL;token=0;
    state=RISC_USB_MSC_IDLE;prepare_steps=0;
    preparing_presented=preparation_blocked=false;
    host_reads=host_writes=0;
    unknown_custody=release_failed=confirm_removed=local_ready=false;
    auto_cleanup_failed=return_after_eject=false;action_gate=true;dirty=true;detail[0]=0;
    checkpoint_client=(risc_diagnostic_checkpoint_client_v1){.struct_size=sizeof(checkpoint_client)};
    if(!risc_runtime_diagnostic_checkpoint_client(runtime,&checkpoint_client) ||
       checkpoint_client.api_version!=1 ||
       checkpoint_client.struct_size<sizeof(checkpoint_client) ||
       !checkpoint_client.invocation || !checkpoint_client.write)
        checkpoint_client=(risc_diagnostic_checkpoint_client_v1){0};
    if(!shared_live()||!shared_probe_msc()||!resident_bind()||!alarm_open())return;
    scene_grant=(risc_runtime_capability_v1){.struct_size=sizeof(scene_grant)};
    if(!runtime->acquire(RISC_SCENE_CAPABILITY,1,0,&scene_grant)) {
        if(scene_grant.api || scene_grant.slot || scene_grant.generation)shared_retain();
        else if(shared_live()) {
            if(runtime->diagnostic)runtime->diagnostic("USB SD Transfer: semantic presentation unavailable");
            if(shared_live())(void)alarm_close();
        }
        return;
    }
    if(!shared_live()){shared_retain();return;}
    scene_owned=true;scene=scene_grant.api;
    components=risc_scene_components_get_v1(scene);
    if(!components || !scene->next || !scene->snapshot || !scene->close ||
       !components->lifecycle.configure) {
        if(runtime->diagnostic)runtime->diagnostic("USB SD Transfer: incompatible semantic presentation");
        if(shared_close() && shared_live())(void)alarm_close();
        return;
    }
    shared_document();painted_state=state;
    risc_scene_navigation_v1 nav={.api_version=1,.struct_size=sizeof(nav),.depth=1,.routes={1}};
    int32_t rc=components->open(scene->context,&document,&nav,&scene_session);
    if(!shared_result(rc)) {
        if(scene_session || rc==RISC_SCENE_AGAIN)shared_retain();
        else if(shared_close() && shared_live())(void)alarm_close();
        return;
    }
    if(!scene_session){shared_retain();return;}
    if(!shared_result(components->lifecycle.configure(scene->context,scene_session,resident.invocation?RISC_SCENE_FEATURE_SHARED_CONTROLS:0))) {
        shared_retain();return;
    }
    dirty=false;
    for(;;) {
        if(!shared_live())return;
        if(release_failed){shared_retain();return;}
        portable_usb_transfer_service();
        if(!shared_live())return;
        if(release_failed){shared_retain();return;}
        if(return_after_eject)closing=true;
        if(closing && !alarm_modal && !alarm_owned()) {
            if(!portable_usb_transfer_owned() && portable_usb_transfer_close()) {
                if(shared_close() && alarm_close()) {
                    if(resident.invocation && !strcmp(destination,"default.elf"))return;
                    if(shared_live())(void)runtime->request_launch(destination);
                    return;
                }
                if(!shared_live())return;
            }
            runtime->yield_ms(2);continue;
        }
        uint32_t flags=0;
        risc_scene_navigation_v1 snapshot={.api_version=1,.struct_size=sizeof(snapshot)};
        rc=scene->snapshot(scene->context,scene_session,&snapshot,&flags);
        if(!shared_result(rc)){shared_retain();return;}
        bool settled=!(flags&(RISC_SCENE_PRESENTING|RISC_SCENE_CLOSING));
        if(settled && !(flags&RISC_SCENE_INPUT_BUSY)) {
            if(!alarm_pump())return;
        }
        if(controls_requested && !portable_usb_transfer_owned() && !alarm_owned() && !alarm_modal) {
            rc=scene->close(scene->context,scene_session);
            if(!shared_live()||rc==RISC_SCENE_RETAINED){shared_retain();return;}
            if(rc==RISC_SCENE_AGAIN){runtime->yield_ms(2);continue;}
            if(rc!=RISC_SCENE_OK){shared_retain();return;}
            scene_session=0;
            risc_resident_request_v1 request={sizeof(request),RISC_RESIDENT_CHECKPOINT_CONTROLS,0,0};
            risc_resident_reply_v1 reply={sizeof(reply),0};
            rc=resident.checkpoint(resident.invocation,&request,&reply);
            if(!shared_live()||rc==RISC_RESIDENT_RETAINED){shared_retain();return;}
            if(rc==RISC_RESIDENT_EXIT) {
                closing=true;destination="default.elf";
                if(shared_close()&&alarm_close())return;
                shared_retain();return;
            }
            if(rc!=RISC_RESIDENT_OK&&rc!=RISC_RESIDENT_BUSY){shared_retain();return;}
            controls_requested=false;action_gate=true;shared_document();painted_state=state;
            if(!shared_result(components->open(scene->context,&document,&nav,&scene_session))||!scene_session) {
                shared_retain();return;
            }
            if(!shared_result(components->lifecycle.configure(scene->context,scene_session,resident.invocation?RISC_SCENE_FEATURE_SHARED_CONTROLS:0))){shared_retain();return;}
            dirty=false;runtime->yield_ms(2);continue;
        }
        risc_scene_event_v1 event={.struct_size=sizeof(event)};
        rc=scene->next(scene->context,scene_session,&event);
        if(!shared_live() || (rc!=RISC_SCENE_OK&&rc!=RISC_SCENE_IDLE)) {
            shared_retain();return;
        }
        bool consumed=false;
        if(rc==RISC_SCENE_OK && event.struct_size>=sizeof(event) && event.sequence>event_sequence) {
            event_sequence=event.sequence;
            if(event.document_revision==document.revision && event.kind==RISC_SCENE_CONTROLS_EVENT &&
               !event.node && !event.action && !event.value && resident.invocation &&
               !portable_usb_transfer_owned() && !alarm_owned() && !alarm_modal) {
                controls_requested=true;consumed=true;action_gate=true;
            }
            if(event.document_revision==document.revision &&
               event.kind==RISC_SCENE_ACTION_EVENT && shared_event_valid(&event)) {
                /* A queued Cancel is handled before preparation, even while
                 * the Preparing frame is still being presented. */
                uint32_t action=event.action;
                bool cancel=state==RISC_USB_MSC_PREPARING && action==USB_START_STOP;
                if(!action_gate || cancel) {
                    consumed=true;action_gate=true;
                    if(action==USB_ALARM_DISMISS && alarm_modal && alarm_status.state==ALARM_STATE_ALERT) {
                        if(!alarm_result(alarm_api->acknowledge(alarm_api->context,&alarm_status.occurrence)))return;
                        dirty=true;
                    } else if(action==USB_ALARM_RETRY && alarm_modal && alarm_status.state==ALARM_STATE_BLOCKED) {
                        if(!alarm_result(alarm_api->refresh(alarm_api->context)))return;
                        dirty=true;
                    } else if(action==USB_CONTROLS && resident.invocation && !portable_usb_transfer_owned() && !alarm_owned())controls_requested=true;
                    else if(action==USB_KEEP && confirm_removed){confirm_removed=false;dirty=true;}
                    else if(action==USB_REMOVED && confirm_removed)
                        (void)end_session(RISC_USB_MSC_END_CABLE_REMOVED,false);
                    else if(action==USB_BACK) {
                        if(confirm_removed){confirm_removed=false;dirty=true;}
                        else if(portable_app_before_launch("springboard.elf")){destination="springboard.elf";closing=true;}
                    } else if(action==USB_START_STOP && !confirm_removed) {
                        if(portable_usb_transfer_owned())
                            (void)end_session(RISC_USB_MSC_END_CANCEL_WAITING,false);
                        else start();
                        dirty=true;
                    }
                }
            }
        }
        if(!shared_live())return;
        if(release_failed){shared_retain();return;}
        if(rc==RISC_SCENE_IDLE && !(flags&RISC_SCENE_INPUT_BUSY))action_gate=false;
        if(settled && dirty && !closing) {
            shared_document();painted_state=state;preparing_presented=false;
            if(!shared_result(components->update(scene->context,scene_session,&document))) {
                shared_retain();return;
            }
            dirty=false;settled=false;
        }
        if(settled && !dirty && painted_state==state)
            preparing_presented=state==RISC_USB_MSC_PREPARING;
        if(!consumed && rc==RISC_SCENE_IDLE && !(flags&RISC_SCENE_INPUT_BUSY) &&
           !closing && !alarm_modal)prepare_once();
        if(!shared_live())return;
        if(release_failed){shared_retain();return;}
        runtime->yield_ms(state==RISC_USB_MSC_CONNECTED?1:2);
    }
}

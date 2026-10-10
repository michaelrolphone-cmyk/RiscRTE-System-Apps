#ifdef PORTABLE_RESIDENT_SHELL_CLIENT
#include "PortableNativeCustody.h"
#endif
/* The computer is the USB host. This app exports the local SD through the
 * generic owner-task capability, without chip, GPIO, USB-stack or SD access. */
#include "T5AppApi.h"
#include "PaperPresentation.h"
#include "PortableAppLaunchGuard.h"
#include "PortableUsbTransfer.h"
#include "RiscRuntimeV1.h"
#include <RiscUsbDeviceMscV1.h>
#include <string.h>
#include <stdio.h>

static const t5_app_api_v1 *app;
static const paper_presentation *paper;
static const risc_runtime_api_v1 *runtime;
static risc_runtime_capability_v1 grant;
static const risc_usb_device_msc_api_v1 *usb;
static const risc_usb_device_msc_api_v1_prepare *preparation;
static uint64_t token;
#ifdef RISC_USB_MSC_DIAGNOSTICS_TAG
static const risc_usb_device_msc_api_v1_diagnostics *protocol_api;
static risc_usb_device_msc_diagnostics_v1 protocol;
static uint64_t logged_started, logged_completed;
#endif
static uint64_t host_reads, host_writes;
static uint32_t state, prepare_steps;
static bool preparing_presented, preparation_blocked;
static const char *status_name(void);
static bool unknown_custody, release_failed, confirm_removed, action_gate, dirty, local_ready, auto_cleanup_failed, return_after_eject;
static char detail[112];
static bool terminal(uint32_t s) {
    return s==RISC_USB_MSC_EJECTED || s==RISC_USB_MSC_DISCONNECTED || s==RISC_USB_MSC_MEDIA_UNAVAILABLE;
}
bool portable_usb_transfer_owned(void) {return token || unknown_custody || release_failed;}
static void error_text(const char *fallback) {
    detail[0]=0;
    if(usb && usb->last_error)usb->last_error(usb->context,detail,sizeof(detail));
    detail[sizeof(detail)-1]=0;
    if(!detail[0]) {size_t n=strlen(fallback);if(n>=sizeof(detail))n=sizeof(detail)-1;memcpy(detail,fallback,n);detail[n]=0;}
    dirty=true;
}
static void log_result(const char *operation,int32_t result) {
    char line[240];
    snprintf(line,sizeof(line),"USB transfer %s: result=%ld state=%s step=%lu%s%s",
        operation,(long)result,status_name(),(unsigned long)prepare_steps,detail[0]?" reason=":"",detail);
    if(runtime->diagnostic)runtime->diagnostic(line);
}
#ifdef RISC_USB_MSC_DIAGNOSTICS_TAG
static const char *command_name(uint32_t opcode) {
    switch(opcode) {
    case 0x00:return "TEST UNIT READY"; case 0x03:return "REQUEST SENSE";
    case 0x12:return "INQUIRY"; case 0x1a:return "MODE SENSE(6)";
    case 0x1b:return "START STOP"; case 0x1e:return "PREVENT ALLOW";
    case 0x23:return "READ FORMAT CAPACITY"; case 0x25:return "READ CAPACITY(10)";
    case 0x28:return "READ(10)"; case 0x2a:return "WRITE(10)";
    case 0x35:return "SYNCHRONIZE CACHE"; default:return "OTHER";
    }
}
/* The provider only copies owner-task RAM here. Diagnostic output uses the
 * existing Runtime logger, which buffers while USB owns the PHY/SD. No file
 * write or console recovery is attempted from this app. Log commands, never
 * individual USB packets or mouse/touch samples. */
static void read_protocol(void) {
    if(!protocol_api || !token)return;
    risc_usb_device_msc_diagnostics_v1 next={.struct_size=sizeof(next)};
    if(protocol_api->diagnostics(usb->context,token,&next)!=RISC_USB_MSC_OK)return;
    protocol=next;
    char line[248];
    if(next.commands_started!=logged_started) {
        snprintf(line,sizeof(line),"USB command begin: name=%s op=%02lx tag=%lu bytes=%lu lba=%lu blocks=%lu start_ms=%llu poll_gap_ms=%llu count=%llu",
            command_name(next.current_opcode),(unsigned long)next.current_opcode,
            (unsigned long)next.current_tag,(unsigned long)next.command_bytes,
            (unsigned long)next.current_lba,(unsigned long)next.current_block_count,
            (unsigned long long)next.command_started_ms,(unsigned long long)next.last_poll_gap_ms,
            (unsigned long long)(next.commands_started-logged_started));
        if(runtime->diagnostic)runtime->diagnostic(line);
        logged_started=next.commands_started;
    }
    if(next.commands_completed!=logged_completed) {
        const char *result=next.last_csw_status==0?"passed":next.last_csw_status==1?"failed":"phase-error";
        snprintf(line,sizeof(line),"USB command end: name=%s tag=%lu result=%s elapsed_ms=%llu io_ms=%llu io_result=%ld read=%llu write=%llu stalls=%lu count=%llu",
            command_name(next.completed_opcode),(unsigned long)next.completed_tag,result,
            (unsigned long long)next.last_command_elapsed_ms,(unsigned long long)next.last_io_elapsed_ms,
            (long)next.last_io_result,(unsigned long long)next.blocks_read,
            (unsigned long long)next.blocks_written,(unsigned long)next.stalls,
            (unsigned long long)(next.commands_completed-logged_completed));
        if(runtime->diagnostic)runtime->diagnostic(line);
        logged_completed=next.commands_completed;
    }
}
#else
static void read_protocol(void) {}
#endif
static bool release_grant(void) {
    if(!grant.api)return true;
    if(!runtime->release(&grant)) {
        release_failed=true;state=RISC_USB_MSC_FAULT_RETAINED;
        error_text("Provider release failed. Retry Stop.");return false;
    }
    grant=(risc_runtime_capability_v1){0};usb=NULL;preparation=NULL;release_failed=false;
#ifdef RISC_USB_MSC_DIAGNOSTICS_TAG
    protocol_api=NULL;
#endif
    return true;
}
static bool end_session(uint32_t reason, bool automatic) {
    if(unknown_custody) {error_text("Ownership is uncertain. Restart required.");return false;}
    if(!token)return release_grant();
    const bool cancelling=state==RISC_USB_MSC_PREPARING;
    log_result(cancelling?"cancel preparation requested":"stop requested",(int32_t)reason);
    int32_t result=usb->end(usb->context,token,reason);
    if(result==RISC_USB_MSC_OK) {
        token=0;confirm_removed=false;
        if(!terminal(state))state=RISC_USB_MSC_DISCONNECTED;
        if(state!=RISC_USB_MSC_MEDIA_UNAVAILABLE)detail[0]=0;
        dirty=true;preparing_presented=preparation_blocked=false;log_result("stopped",result);return release_grant();
    }
    if(cancelling)preparation_blocked=true;
    if(automatic)auto_cleanup_failed=true;
    error_text("Cleanup is incomplete. Keep this screen open.");
    if(result==RISC_USB_MSC_REFUSED && !automatic && !cancelling) {
        confirm_removed=true;action_gate=true;
    } else if(result!=RISC_USB_MSC_REFUSED)state=RISC_USB_MSC_FAULT_RETAINED;
    log_result("cleanup incomplete",result);
    return false;
}
void portable_usb_transfer_service(void) {
    if(!usb || !token)return;
    risc_usb_device_msc_status_v1 status={.struct_size=sizeof(status)};
    int32_t result=usb->poll(usb->context,token,&status);
    read_protocol();
    const bool had_media_io=host_reads || host_writes;
    host_reads=status.blocks_read;host_writes=status.blocks_written;
    if(!had_media_io && (host_reads || host_writes)) {
        dirty=true;log_result("host SD block access confirmed",result);
    }
    if(result!=RISC_USB_MSC_OK && result!=RISC_USB_MSC_RETAINED) {
        preparation_blocked=true;preparing_presented=false;
        if(state!=RISC_USB_MSC_FAULT_RETAINED) {
            state=RISC_USB_MSC_FAULT_RETAINED;error_text("USB status unavailable. Stop and check the cable.");
            log_result("status failed",result);
        }
        return;
    }
    local_ready=status.local_media_ready!=0;
    uint32_t next=status.state;
    if(next>RISC_USB_MSC_PREPARING || next==RISC_USB_MSC_IDLE || result==RISC_USB_MSC_RETAINED)
        next=RISC_USB_MSC_FAULT_RETAINED;
    if(next!=state){
        state=next;dirty=true;
        if(next==RISC_USB_MSC_FAULT_RETAINED){preparation_blocked=true;error_text("Cleanup is incomplete. Retry Stop.");}
        if(next==RISC_USB_MSC_MEDIA_UNAVAILABLE)error_text("SD preparation failed. Check the card and retry.");
        log_result("state changed",result);
    }
    /* Only provider-confirmed eject/disconnect runs automatic cleanup. USB
     * suspend, reset/unconfigure and missing SOF never prove cable removal. */
    if(terminal(state) && !auto_cleanup_failed) {
        const bool ejected=state==RISC_USB_MSC_EJECTED;
        if(end_session(RISC_USB_MSC_END_CANCEL_WAITING,true) && ejected)return_after_eject=true;
    }
}
bool portable_usb_transfer_close(void) {
    if(token || unknown_custody)return false;
    return release_grant();
}
bool portable_app_before_launch(const char *destination) {
    (void)destination;
    if(portable_usb_transfer_owned()) {
        if(!confirm_removed)error_text("Stop the transfer before leaving this screen.");
        return false;
    }
    return portable_usb_transfer_close();
}
static void start(void) {
    if(portable_usb_transfer_owned())return;
    confirm_removed=local_ready=auto_cleanup_failed=preparing_presented=preparation_blocked=false;prepare_steps=0;detail[0]=0;
    host_reads=host_writes=0;
#ifdef RISC_USB_MSC_DIAGNOSTICS_TAG
    protocol_api=NULL;protocol=(risc_usb_device_msc_diagnostics_v1){0};logged_started=logged_completed=0;
#endif
    grant=(risc_runtime_capability_v1){.struct_size=sizeof(grant)};
    if(!runtime->acquire(RISC_USB_DEVICE_MSC_CAPABILITY,1,0,&grant)) {
        state=RISC_USB_MSC_MEDIA_UNAVAILABLE;error_text("USB transfer is unavailable. Retry Start.");return;
    }
    usb=grant.api;preparation=risc_usb_device_msc_prepare(usb);
    if(!preparation || !usb || usb->api_version!=1 || usb->struct_size<sizeof(*usb) || !usb->begin || !usb->poll || !usb->end || !usb->last_error) {
        usb=NULL;state=RISC_USB_MSC_MEDIA_UNAVAILABLE;error_text("USB transfer provider is incompatible.");release_grant();return;
    }
    #ifdef RISC_USB_MSC_DIAGNOSTICS_TAG
    protocol_api=risc_usb_device_msc_diagnostics(usb);
#endif
    token=0;log_result("begin requested",0);int32_t result=usb->begin(usb->context,&token);
    if(result==RISC_USB_MSC_OK && token) {state=RISC_USB_MSC_PREPARING;dirty=true;log_result("SD lease acquired",result);return;}
    unknown_custody=(result==RISC_USB_MSC_RETAINED || result==RISC_USB_MSC_OK) && !token;
    if(token || unknown_custody){state=RISC_USB_MSC_FAULT_RETAINED;preparation_blocked=true;}
    else state=RISC_USB_MSC_MEDIA_UNAVAILABLE;
    error_text("Close local files and retry with an inserted SD card.");
    log_result("begin failed",result);
    if(!token && !unknown_custody)release_grant();
}
/* This is deliberately separate from service(): display/input waits may poll
 * status, but never perform an SD preparation transaction or claim the PHY. */
static void prepare_once(void) {
    if(!preparation || !token || state!=RISC_USB_MSC_PREPARING || !preparing_presented || preparation_blocked || dirty)return;
    ++prepare_steps;log_result("prepare step requested",0);
    const int32_t result=preparation->prepare_step(usb->context,token);
    if(result!=RISC_USB_MSC_OK) {
        preparation_blocked=true;
        error_text("SD preparation failed. Cancel and retry.");
        log_result("prepare step failed",result);
        if(result==RISC_USB_MSC_RETAINED) {
            state=RISC_USB_MSC_FAULT_RETAINED;dirty=true;preparing_presented=false;
        } else {
            /* Clean preparation errors still own their session until end
             * confirms cancellation. Preserve the actual reason for retry. */
            char reason[sizeof(detail)];memcpy(reason,detail,sizeof(reason));
            if(end_session(RISC_USB_MSC_END_CANCEL_WAITING,true)) {
                state=RISC_USB_MSC_MEDIA_UNAVAILABLE;memcpy(detail,reason,sizeof(detail));dirty=true;
            }
        }
        return;
    }
    portable_usb_transfer_service();log_result("prepare step completed",result);
}
static int sx(int x){return x*app->screen_width()/480;}
static int sy(int y){return y*app->screen_height()/800;}
static void text(int y,const char *s,bool heading) {paper->text(sx(32),sy(y),sx(416),s,(heading?2u:1u)|PAPER_TEXT_FIT,heading,true);}
static void wrap(int y,const char *s) {
    for(unsigned row=0;row<3 && *s;row++) {
        char line[112];size_t n=0,space=0;
        while(s[n] && n+1<sizeof(line)) {
            line[n]=s[n];line[n+1]=0;
            if(paper->measure(line,false)>sx(416)){line[n]=0;break;}
            if(s[n]==' ')space=n;
            n++;
        }
        if(!n)break;
        if(s[n] && space){n=space;line[n]=0;}
        text(y+(int)row*32,line,false);s+=n;while(*s==' ')s++;
    }
}
static void button(int x,int y,int w,const char *label) {
    app->fill_rect(sx(x),sy(y),sx(w),sy(68),true);
    app->fill_rect(sx(x+3),sy(y+3),sx(w-6),sy(62),false);
    paper->text(sx(x+12),sy(y+22),sx(w-24),label,1|PAPER_TEXT_FIT,false,true);
}
static const char *status_name(void) {
    switch(state) {
    case RISC_USB_MSC_PREPARING:return "PREPARING SD";
    case RISC_USB_MSC_WAITING:return "WAITING FOR USB";
    case RISC_USB_MSC_CONNECTED:return host_reads || host_writes ? "SD ACCESS CONFIRMED" : "USB CONFIGURED";
    case RISC_USB_MSC_SUSPENDED:return "USB SUSPENDED";
    case RISC_USB_MSC_EJECTED:return "SAFELY EJECTED";
    case RISC_USB_MSC_DISCONNECTED:return "DISCONNECTED";
    case RISC_USB_MSC_MEDIA_UNAVAILABLE:return "MEDIA UNAVAILABLE";
    case RISC_USB_MSC_FAULT_RETAINED:return "CLEANUP REQUIRED";
    default:return "READY TO TRANSFER";
    }
}
static void render(void) {
    /* Clear before drawing: servicing a display wait may set dirty again. */
    const uint32_t painted_state=state;
    dirty=false;paper->begin();text(28,"USB SD TRANSFER",true);
    app->fill_rect(sx(32),sy(82),sx(416),3,true);
    text(116,status_name(),true);
    if(confirm_removed) {
        text(202,"Eject the SD drive on your computer.",false);
        text(252,"If eject is unavailable, stop all copies",false);
        text(294,"and physically unplug the USB cable.",false);
        text(364,"CABLE REMOVED?",true);
        text(422,"Unplugging during a write can lose files.",false);
        if(state==RISC_USB_MSC_FAULT_RETAINED && detail[0])wrap(474,detail);
        button(32,612,416,"CABLE REMOVED");button(32,704,416,"KEEP TRANSFERRING");
    } else {
        if(state==RISC_USB_MSC_PREPARING) {
            text(202,"Finishing pending device log writes.",false);
            text(252,"The SD drive appears when preparation ends.",false);
            text(294,"Keep this screen open, or cancel below.",false);
            button(32,612,416,"CANCEL PREPARATION");
        } else if(portable_usb_transfer_owned()) {
            text(202,state==RISC_USB_MSC_FAULT_RETAINED?"SD remains reserved until cleanup succeeds.":state==RISC_USB_MSC_WAITING?"Connect the USB data cable to a computer.":(host_reads || host_writes)?"The computer has accessed SD sectors.":"Waiting for computer disk commands.",false);
            text(252,"Eject the drive after any copies finish.",false);
            text(294,"Keep this screen open during transfer.",false);
            if(state==RISC_USB_MSC_SUSPENDED)text(356,"SD is still shared. Wake your computer.",false);
            if(state==RISC_USB_MSC_FAULT_RETAINED)text(356,"Retry Stop. If it fails, restart the device.",false);
            button(32,612,416,"STOP TRANSFER");
        } else {
            text(202,"Connect a USB data cable to a computer.",false);
            text(252,"Tap Start to share this device's SD card.",false);
            text(294,"Eject the drive before disconnecting.",false);
            if(state==RISC_USB_MSC_MEDIA_UNAVAILABLE)text(356,"Check the card and retry Start.",false);
            else if(terminal(state))text(356,local_ready?"Transfer ended. SD is available locally.":"Transfer ended. USB ownership released.",false);
            button(32,612,416,"START TRANSFER");
        }
        if(detail[0])wrap(440,detail);
        text(544,"Copy x4-boot.log and x4-boot.previous.log",false);
        text(576,"from the USB drive to get device logs.",false);
        button(32,704,416,portable_usb_transfer_owned()?"HOME LOCKED DURING TRANSFER":"BACK TO APPS");
    }
    app->present(false);
    preparing_presented=painted_state==RISC_USB_MSC_PREPARING && state==painted_state && !dirty;
}
static bool hit(const t5_app_input_t *in,int y) {
    return in->tapped && in->touch_x>=sx(32) && in->touch_x<sx(448) && in->touch_y>=sy(y) && in->touch_y<sy(y+68);
}
void app_main(void) {
    app=t5_app_get_api(1);paper=paper_presentation_get();runtime=risc_runtime_get_api(1);
    if(!app || !paper || !runtime || !runtime->acquire || !runtime->release || !runtime->request_launch)return;
    grant=(risc_runtime_capability_v1){0};usb=NULL;preparation=NULL;token=0;state=RISC_USB_MSC_IDLE;prepare_steps=0;preparing_presented=preparation_blocked=false;
    host_reads=host_writes=0;unknown_custody=release_failed=confirm_removed=local_ready=auto_cleanup_failed=return_after_eject=false;action_gate=true;dirty=true;detail[0]=0;
    app->set_back_exits_app(false);
    for(;;) {
#ifdef PORTABLE_RESIDENT_SHELL_CLIENT
        if(portable_adapter_retained())return;
#endif
        portable_usb_transfer_service();
        /* Service may run inside display/input waits; navigation belongs to
         * this outer loop only after checked session and provider cleanup. */
        if(return_after_eject) {
            return_after_eject=false;
            if(portable_app_before_launch("default.elf")) {
#ifdef PORTABLE_RESIDENT_SHELL_CLIENT
                break; /* Restore the already mapped resident Home host. */
#else
                if(runtime->request_launch("default.elf"))break;
#endif
            }
        }
        if(dirty)render();
        if(return_after_eject)continue;
        t5_app_input_t in={0};
        if(!app->poll(&in,2)) {
#ifdef PORTABLE_RESIDENT_SHELL_CLIENT
            if(portable_adapter_retained())return;
#endif
            if(!portable_usb_transfer_close()) {runtime->yield_ms(2);continue;}
            break;
        }
        if(return_after_eject)continue;
        springboard_contact contact={0};paper->contact(&contact);
        if(contact.valid && contact.released && contact.tap_eligible && !contact.cancelled) {
            in.tapped=true;in.touch_x=contact.x;in.touch_y=contact.y;
        }

        if(in.exit_requested && !portable_usb_transfer_owned())break;
        /* A completed Cancel tap captured during the preparing frame must
         * win before the first SD transaction, even while Start is gated. */
        if(state==RISC_USB_MSC_PREPARING && hit(&in,612)) {
            action_gate=true;end_session(RISC_USB_MSC_END_CANCEL_WAITING,false);dirty=true;continue;
        }
        if(action_gate) {if(!in.buttons && !in.tapped && contact.valid && !contact.down)action_gate=false;continue;}
        if((in.buttons&T5_APP_BUTTON_BACK) || hit(&in,704)) {
            if(confirm_removed){confirm_removed=false;dirty=true;action_gate=true;continue;}
            if(portable_app_before_launch("springboard.elf") && runtime->request_launch("springboard.elf"))break;
            dirty=true;continue;
        }
        if((in.buttons&T5_APP_BUTTON_CONFIRM) || hit(&in,612)) {
            action_gate=true;
            if(confirm_removed)end_session(RISC_USB_MSC_END_CABLE_REMOVED,false);
            else if(portable_usb_transfer_owned())end_session(RISC_USB_MSC_END_CANCEL_WAITING,false);
            else start();
            dirty=true;continue;
        }
        if(!contact.down)prepare_once();
    }
}

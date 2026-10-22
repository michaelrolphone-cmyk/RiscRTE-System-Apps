/* Unmanifested, local-only native Serial Monitor candidate. No product binding,
 * role switching, hardware discovery, raw serial fallback or launch route. */
#include "PortableSerialMonitor.h"
#include "PortableSerialClient.h"
#include "PortableNativeCustody.h"
#include "PortableAppLaunchGuard.h"
#include "T5AppFrame.h"
#include <stdio.h>
#include <string.h>
#if !defined(PORTABLE_SERIAL_MONITOR_APP) || !defined(PORTABLE_SERIAL_STREAMS)
#error "Serial Monitor must be explicitly selected with the stream client"
#endif
#include "serial_monitor_font.inc"
#define SM_LINES 64u
#define SM_COLUMNS 80u
#define SM_ROW 24
#define SM_TOP 56
#define SM_PERIOD 250u
static const t5_app_api_v1 *sm_app;
static const risc_runtime_api_v1 *sm_runtime;
static portable_serial_client sm_client;
static bool sm_started,sm_client_ready,sm_dirty,sm_input_armed,sm_urgent;
static uint32_t sm_buttons,sm_last_frame;
static unsigned sm_mode,sm_selected,sm_first,sm_width,sm_height,sm_columns;
static unsigned sm_head,sm_count,sm_column,sm_scroll,sm_baud;
static char sm_lines[SM_LINES][SM_COLUMNS+1u],sm_status[96];
static const uint32_t sm_bauds[]={9600,19200,38400,57600,115200,230400,460800,921600};
enum {SM_PROVIDERS,SM_ENDPOINTS,SM_TERMINAL,SM_UNAVAILABLE};
static bool sm_safe(void) {
    if(portable_adapter_retained())return false;
    if(sm_client_ready && sm_client.cleanup_required){portable_adapter_retain_silent();return false;}
    return true;
}
bool portable_serial_monitor_close(void) {
    if(!sm_safe())return false;
    if(!sm_client_ready)return true;
    if(!portable_serial_close(&sm_client)) {
        /* A stale/uncertain close is not retryable by the UI. */
        portable_adapter_retain_silent();return false;
    }
    sm_client_ready=false;return true;
}
bool portable_app_before_launch(const char *destination) {
    (void)destination;return portable_serial_monitor_close();
}
static void sm_message(const char *text) {
    snprintf(sm_status,sizeof(sm_status),"%s",text);sm_dirty=true;
}
static const char *sm_error(int code) {
    switch(code) {
    case PSC_DENIED:return "Provider unavailable/denied";
    case PSC_UNSUPPORTED:return "Stream API unavailable";
    case PSC_DISCONNECTED:return "Disconnected - select again";
    case PSC_BUSY:return "Busy - no automatic retry";
    case PSC_EOF:return "End of stream - select again";
    case PSC_IO:return "Inventory/stream unavailable";
    case PSC_TIMEOUT:return "Timed out";
    default:return "Serial operation failed";
    }
}
static void sm_page(unsigned mode) {
    sm_mode=mode;sm_selected=sm_first=0;sm_dirty=sm_urgent=true;sm_input_armed=false;
}
static unsigned sm_rows(void) {return (sm_height-100u)/SM_ROW;}
static unsigned sm_items(void) {
    if(sm_mode==SM_PROVIDERS)return portable_serial_monitor_instance_count;
    if(sm_mode==SM_ENDPOINTS)return 1u+(unsigned)sm_client.inventory_count;
    return 0;
}
static unsigned sm_visible_lines(void) {return (sm_height-98u)/14u;}
static unsigned sm_max_scroll(void) {
    unsigned visible=sm_visible_lines();return sm_count>visible?sm_count-visible:0;
}
static void sm_clear_terminal(void) {
    memset(sm_lines,0,sizeof(sm_lines));sm_head=sm_column=sm_scroll=0;sm_count=1;sm_dirty=true;
}
static void sm_newline(void) {
    sm_head=(sm_head+1u)%SM_LINES;sm_lines[sm_head][0]=0;sm_column=0;
    if(sm_count<SM_LINES)++sm_count;
    if(sm_scroll && sm_scroll<sm_max_scroll())++sm_scroll;
}
static void sm_character(char c) {
    if(sm_column>=sm_columns)sm_newline();
    char *line=sm_lines[sm_head];size_t length=strlen(line);
    line[sm_column++]=c;if(sm_column>length)line[sm_column]=0;
}
static void sm_receive(const unsigned char *bytes,size_t n) {
    static const char hex[]="0123456789ABCDEF";
    for(size_t i=0;i<n;i++) {
        unsigned c=bytes[i];
        if(c=='\n')sm_newline();
        else if(c=='\r')sm_column=0;
        else if(c=='\b') {
            if(sm_column) {
                --sm_column;
                char *line=sm_lines[sm_head];
                /* Erase one cell without discarding later text left by CR. */
                line[sm_column]=line[sm_column+1u]?' ':0;
            }
        }
        else if(c=='\t') {unsigned spaces=4u-sm_column%4u;while(spaces--)sm_character(' ');}
        else if(c>=32 && c<127)sm_character((char)c);
        else {sm_character('[');sm_character(hex[c>>4]);sm_character(hex[c&15]);sm_character(']');}
    }
    sm_dirty=true;
}
static void sm_text(int x,int y,const char *text) {
    /* Own ASCII renderer preserves case/punctuation unlike the compact adapter
     * labels. Only bounded framebuffer primitives, never terminal escapes. */
    for(unsigned i=0;i<SM_COLUMNS && text[i] && x+8<=(int)sm_width;i++,x+=8) {
        unsigned c=(unsigned char)text[i];if(c<32 || c>126)c='?';
        for(unsigned row=0;row<12 && y+(int)row<(int)sm_height;row++)
            for(unsigned col=0;col<8;col++)if(sm_font[c-32][row]&(1u<<(7-col))) {
                if(!sm_safe())return;
                sm_app->fill_rect(x+(int)col,y+(int)row,1,1,true);
            }
    }
}
static const char *sm_transport(unsigned kind) {
    static const char *const names[]={"Internal","UART","USB","BLE","I2C","SPI","GPIO","LoRa","IP"};
    return kind<sizeof(names)/sizeof(*names)?names[kind]:"Unknown";
}
static void sm_render(void) {
    if(!sm_safe())return;
    if(!t5_app_frame_ready(sm_app) || !sm_safe())return;
    sm_app->clear();if(!sm_safe())return;
    sm_text(8,8,"Back");sm_text(64,8,"Serial Monitor");
    if(sm_mode==SM_ENDPOINTS || sm_mode==SM_TERMINAL)sm_text((int)sm_width-64,28,sm_mode==SM_TERMINAL?"Clear":"Refresh");
    char text[96];
    if(sm_mode==SM_TERMINAL) {
        snprintf(text,sizeof(text),"RX %llu bytes",(unsigned long long)sm_client.rx_bytes);
        sm_text(8,28,text);
        unsigned visible=sm_visible_lines(),end=sm_count-sm_scroll;
        unsigned start=end>visible?end-visible:0;
        unsigned oldest=(sm_head+SM_LINES+1u-sm_count)%SM_LINES;
        for(unsigned i=start;i<end;i++)sm_text(8,SM_TOP+(int)(i-start)*14,sm_lines[(oldest+i)%SM_LINES]);
    } else if(sm_mode==SM_PROVIDERS || sm_mode==SM_ENDPOINTS) {
        unsigned items=sm_items(),visible=sm_rows();sm_first=sm_selected/visible*visible;
        sm_text(8,28,sm_mode==SM_PROVIDERS?"Choose provider instance":"Choose endpoint");
        for(unsigned i=sm_first;i<items && i<sm_first+visible;i++) {
            int y=SM_TOP+(int)(i-sm_first)*SM_ROW;
            if(sm_mode==SM_PROVIDERS)
                snprintf(text,sizeof(text),"%llu %s",(unsigned long long)portable_serial_monitor_instances[i].instance,portable_serial_monitor_instances[i].label);
            else if(!i)snprintf(text,sizeof(text),"Baud %lu 8N1 - change",(unsigned long)sm_bauds[sm_baud]);
            else snprintf(text,sizeof(text),"Endpoint %u - %s",i,sm_transport(sm_client.inventory[i-1].transport));
            sm_text(20,y,text);
            if(i==sm_selected){if(!sm_safe())return;sm_app->fill_rect(8,y+2,4,8,true);}
        }
    }
    sm_text(8,(int)sm_height-38,sm_status);
    sm_text(8,(int)sm_height-18,sm_mode==SM_TERMINAL?"Older":"Prev");
    sm_text((int)sm_width/3+8,(int)sm_height-18,sm_mode==SM_TERMINAL?"Live":"Select");
    sm_text((int)sm_width*2/3+8,(int)sm_height-18,sm_mode==SM_TERMINAL?"Newer":"Next");
    if(!sm_safe())return;
    sm_app->present(false);
    if(!sm_safe())return;
    sm_dirty=sm_urgent=false;
}
static bool sm_refresh(void) {
    bool ok=portable_serial_refresh(&sm_client);if(!sm_safe())return false;
    /* A failed refresh keeps the client's prior snapshot for custody, but the
     * UI refuses endpoint activation until a complete fresh snapshot succeeds. */
    if(!ok){sm_message(sm_error(sm_client.result));return false;}
    sm_selected=sm_first=0;sm_message(sm_client.inventory_count?"Select an endpoint":"No endpoints - Refresh");return true;
}
static bool sm_snapshot_valid;
static void sm_activate(unsigned index) {
    if(sm_mode==SM_PROVIDERS) {
        if(index>=portable_serial_monitor_instance_count || !portable_serial_monitor_close())return;
        if(!portable_serial_init_instance(&sm_client,sm_runtime,portable_serial_monitor_instances[index].instance)) {
            sm_message("Stream API unavailable");return;
        }
        sm_client_ready=true;
        (void)portable_serial_set_control_budget(&sm_client,20);
        sm_snapshot_valid=sm_refresh();if(!sm_safe())return;sm_page(SM_ENDPOINTS);
    } else if(sm_mode==SM_ENDPOINTS) {
        if(!index){sm_baud=(sm_baud+1u)%(sizeof(sm_bauds)/sizeof(*sm_bauds));sm_dirty=true;return;}
        if(!sm_snapshot_valid || index>sm_client.inventory_count)return;
        t5_serial_config_t config={sm_bauds[sm_baud],8,T5_SERIAL_PARITY_NONE,1,T5_SERIAL_FLOW_NONE};
        if(!portable_serial_open(&sm_client,index-1u,&config)) {
            if(!sm_safe())return;
            sm_snapshot_valid=false;sm_message(sm_error(sm_client.result));return;
        }
        sm_clear_terminal();sm_page(SM_TERMINAL);sm_message("Receiving");
    }
}
static bool sm_back(void) {
    if(sm_mode==SM_PROVIDERS || sm_mode==SM_UNAVAILABLE)return true;
    if(!portable_serial_monitor_close())return true;
    sm_page(SM_PROVIDERS);sm_message("Choose provider instance");return false;
}
static bool sm_event(const t5_app_input_t *in) {
    if(in->exit_requested)return true;
    uint32_t pressed=in->buttons&~sm_buttons;sm_buttons=in->buttons;
    if(!sm_input_armed){if(!in->buttons && !in->tapped)sm_input_armed=true;return false;}
    if(pressed || in->tapped)sm_urgent=true;
    if(pressed&T5_APP_BUTTON_BACK)return sm_back();
    if(in->tapped) {
        sm_input_armed=false;
        if(in->touch_x<0 || in->touch_y<0 || in->touch_x>=(int)sm_width || in->touch_y>=(int)sm_height)return false;
        if(in->touch_y>=(int)sm_height-24) {
            unsigned zone=(unsigned)in->touch_x*3u/sm_width;
            if(sm_mode==SM_TERMINAL) {
                if(zone==0 && sm_scroll<sm_max_scroll())++sm_scroll;
                else if(zone==1)sm_scroll=0;
                else if(zone==2 && sm_scroll)--sm_scroll;
            } else if(sm_items()) {
                unsigned n=sm_items();
                if(zone==0)sm_selected=sm_selected?sm_selected-1u:n-1u;
                else if(zone==1)sm_activate(sm_selected);
                else sm_selected=(sm_selected+1u)%n;
            }
            sm_dirty=true;return false;
        }
        if(in->touch_y<24 && in->touch_x<60)return sm_back();
        if(in->touch_y>=24 && in->touch_y<48 && in->touch_x>=(int)sm_width-72) {
            if(sm_mode==SM_TERMINAL)sm_clear_terminal();
            else if(sm_mode==SM_ENDPOINTS)sm_snapshot_valid=sm_refresh();
            return false;
        }
        unsigned row=(unsigned)(in->touch_y-SM_TOP)/SM_ROW;
        if(in->touch_y>=SM_TOP && row<sm_rows() && sm_first+row<sm_items()) {
            sm_selected=sm_first+row;sm_activate(sm_selected);
        }
        return false;
    }
    if(sm_mode==SM_TERMINAL) {
        if((pressed&(T5_APP_BUTTON_UP|T5_APP_BUTTON_LEFT)) && sm_scroll<sm_max_scroll())++sm_scroll;
        if((pressed&(T5_APP_BUTTON_DOWN|T5_APP_BUTTON_RIGHT)) && sm_scroll)--sm_scroll;
        if(pressed)sm_dirty=true;
    } else {
        unsigned n=sm_items();if(!n)return false;
        if(pressed&(T5_APP_BUTTON_UP|T5_APP_BUTTON_LEFT)){sm_selected=sm_selected?sm_selected-1:n-1;sm_dirty=true;}
        if(pressed&(T5_APP_BUTTON_DOWN|T5_APP_BUTTON_RIGHT)){sm_selected=(sm_selected+1)%n;sm_dirty=true;}
        if(pressed&T5_APP_BUTTON_CONFIRM){sm_activate(sm_selected);sm_input_armed=false;}
    }
    return false;
}
void app_main(void) {
    if(sm_started || !sm_safe())return;
    sm_started=true;
    sm_runtime=risc_runtime_get_api(1);
    if(!sm_runtime || sm_runtime->struct_size<RISC_RUNTIME_STREAM_CLIENT_V1_SIZE || !sm_runtime->retain_invocation) {sm_started=false;return;}
    sm_app=t5_app_get_api(1);
    if(!sm_app || sm_app->struct_size<offsetof(t5_app_api_v1,set_back_exits_app)+sizeof(sm_app->set_back_exits_app) ||
       !sm_app->screen_width || !sm_app->screen_height || !sm_app->clear || !sm_app->fill_rect || !sm_app->present ||
       !sm_app->poll || !sm_app->millis || !sm_app->set_back_exits_app) {sm_started=false;return;}
    int width=sm_app->screen_width();if(!sm_safe())return;
    int height=sm_app->screen_height();if(!sm_safe())return;
    if(width<160 || height<160 || width>1024 || height>1024){sm_started=false;return;}
    sm_width=(unsigned)width;sm_height=(unsigned)height;sm_columns=(sm_width-16u)/8u;if(sm_columns>SM_COLUMNS)sm_columns=SM_COLUMNS;
    sm_app->set_back_exits_app(false);if(!sm_safe())return;
    bool valid=portable_serial_monitor_instance_count>0 && portable_serial_monitor_instance_count<=PORTABLE_SERIAL_MONITOR_MAX_INSTANCES;
    for(unsigned i=0;valid && i<portable_serial_monitor_instance_count;i++) {
        if(!portable_serial_monitor_instances[i].instance || !portable_serial_monitor_instances[i].label)valid=false;
        for(unsigned j=0;j<i;j++)if(portable_serial_monitor_instances[j].instance==portable_serial_monitor_instances[i].instance)valid=false;
    }
    sm_baud=4;sm_buttons=0;sm_page(valid?SM_PROVIDERS:SM_UNAVAILABLE);sm_message(valid?"No endpoint is opened":"Instance catalog required");
    sm_last_frame=sm_app->millis();if(!sm_safe())return;sm_render();
    while(sm_safe()) {
        t5_app_input_t in={0};bool ok=sm_app->poll(&in,20);if(!sm_safe())return;
        if(!ok || sm_event(&in))break;
        if(!sm_safe())return;
        uint32_t now=sm_app->millis();if(!sm_safe())return;
        if(sm_mode==SM_TERMINAL && sm_client.connected) {
            unsigned char bytes[PSC_CHUNK];size_t n=portable_serial_tick(&sm_client,bytes,sizeof(bytes),now);
            if(!sm_safe())return;
            if(n)sm_receive(bytes,n);
            if(sm_client.result!=PSC_OK)sm_message(sm_error(sm_client.result));
        }
        if(sm_dirty && (sm_urgent || (uint32_t)(now-sm_last_frame)>=SM_PERIOD)) {sm_render();if(!sm_safe())return;sm_last_frame=now;}
    }
    if(!portable_serial_monitor_close())return;
    sm_started=false;
}

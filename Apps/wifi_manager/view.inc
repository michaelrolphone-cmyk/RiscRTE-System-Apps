/* Values and intent IDs only. Rendering is exclusively in ui.scene. */
static unsigned node(wifi_manager *a,unsigned kind,unsigned action_id,const char *label,const char *text){
    unsigned i=a->next_doc.node_count++;risc_scene_node_v1 *n=&a->next_doc.nodes[i];
    *n=(risc_scene_node_v1){.id=i+1,.route=1,.kind=kind,.action=action_id,.step=1};copy_text(n->label,sizeof(n->label),label);copy_text(n->text,sizeof(n->text),text);return i;
}
static void button_node(wifi_manager *a,unsigned id,const char *text,bool primary){unsigned i=node(a,RISC_SCENE_ACTION,id,text,"");if(primary)a->next_doc.nodes[i].flags=RISC_SCENE_PRIMARY;}
static void stat(wifi_manager *a,const char *name,const char *value){node(a,RISC_COMPONENT_STAT_ROW,0,name,value);}
static void section(wifi_manager *a,const char *s){node(a,RISC_COMPONENT_SECTION,0,s,"");}
static void ipv4_text(char *s,size_t n,const uint8_t *p){snprintf(s,n,"%u.%u.%u.%u",p[0],p[1],p[2],p[3]);}
static void tabs(wifi_manager *a,unsigned selected){unsigned i=node(a,RISC_COMPONENT_TAB_BAR,TAB,"","");a->next_doc.nodes[i].maximum=1;a->next_doc.nodes[i].value=(int)selected;copy_text(a->next_doc.details[i].choices,72,"WI-FI|SAVED");a->next_doc.details[i].symbol=RISC_SYMBOL_WIFI;}
static const char *quality(unsigned bars){static const char *const q[]={"NO SIGNAL","POOR","FAIR","GOOD","EXCELLENT"};return q[bars<5?bars:0];}
static void declare(wifi_manager *a){
    a->next_doc=(risc_components_document_v1){.api_version=1,.struct_size=sizeof(a->next_doc),.root=1,.route_count=1,.screen_key=a->screen_key,.flags=RISC_COMPONENTS_TELEMETRY|RISC_COMPONENTS_CONNECTIVITY};
    a->next_doc.routes[0]=(risc_scene_route_v1){1,BACK,"WI-FI"};
    a->next_doc.toast_token=a->toast;copy_text(a->next_doc.toast,40,a->feedback);
    a->plot=(risc_scene_telemetry_v1){.struct_size=sizeof(a->plot)};
    char b[72],c[40];unsigned i;
    if(a->upgrade){a->next_doc.flags=0;node(a,RISC_COMPONENT_EMPTY,0,"UPDATE FIRMWARE","Wi-Fi Manager needs scene 0.5.0 or newer.");button_node(a,BACK,"BACK",false);return;}
    if(a->screen==MAIN){
        a->next_doc.flags|=RISC_COMPONENTS_ROOT_HEADER;
        if(a->enabled){i=node(a,RISC_COMPONENT_HEADER_ACTION,SCAN,"RESCAN","");a->next_doc.details[i].symbol=RISC_SYMBOL_REFRESH;}
        i=node(a,RISC_COMPONENT_SWITCH,TOGGLE,"WI-FI","");a->next_doc.nodes[i].maximum=1;a->next_doc.nodes[i].value=a->enabled;
        if(!a->enabled){node(a,RISC_COMPONENT_EMPTY,0,"WI-FI IS OFF",a->message[0]?a->message:"Turn it on to see and join nearby networks.");button_node(a,TOGGLE,"TURN ON WI-FI",true);}
        else if(!a->radio){node(a,RISC_COMPONENT_EMPTY,0,"PROVIDER UNAVAILABLE","The asynchronous Wi-Fi provider is required.");}
        else{
            if(a->link_up){
                snprintf(b,sizeof(b),"CONNECTED / CH %u / %s",a->connected.channel,auth_name(a->connected.auth));
                i=node(a,RISC_COMPONENT_CONNECTION_CARD,SELECT_CURRENT,a->connected.ssid,b);a->next_doc.nodes[i].value=(int)strength(a->connected_rssi);a->next_doc.details[i].symbol=RISC_SYMBOL_WIFI;
                ipv4_text(c,sizeof(c),a->progress.station);
                snprintf(a->next_doc.details[i].choices,72,"%d DBM|NOT REPORTED|%s|%lu MIN",(int)a->connected_rssi,c,(unsigned long)((uint32_t)(a->now-a->connected_at)/60000));
            }else{i=node(a,RISC_COMPONENT_CARD,0,a->scanning?"SCANNING...":a->stopping?"STOPPING...":"NOT CONNECTED","2.4 GHz / on-demand");a->next_doc.details[i].symbol=RISC_SYMBOL_WIFI;}
            section(a,a->link_up?"OTHER NETWORKS":"AVAILABLE NETWORKS");
            if(a->scanning){i=node(a,RISC_COMPONENT_PROGRESS,0,"", "Scanning for nearby access points");a->next_doc.nodes[i].value=(int)((a->now/1000)%3);}
            else{
                unsigned shown=0;for(unsigned k=0;k<a->scan.count;k++){
                    const garden_radio_scan_entry_v1 *e=&a->scan.entries[k];if(a->link_up&&!strcmp(e->ssid,a->connected.ssid))continue;
                    i=node(a,RISC_COMPONENT_NETWORK_ROW,e->ssid[0]?SELECT_SCAN+k:0,e->ssid[0]?e->ssid:"HIDDEN NETWORK",auth_name(e->auth));a->next_doc.nodes[i].value=(int)strength(e->rssi);a->next_doc.details[i].symbol=e->auth?RISC_SYMBOL_LOCK:RISC_SYMBOL_UNLOCK;
                    for(unsigned z=0;z<a->saved_rows;z++)if(!strcmp(a->saved_names[z],e->ssid))copy_text(a->next_doc.details[i].badge,16,"SAVED");shown++;
                }
                if(!shown&&!a->have_scan)button_node(a,SCAN,"SCAN FOR NETWORKS",true);
                else if(!shown)node(a,RISC_COMPONENT_EMPTY,0,"NO NETWORKS FOUND","Rescan or add a hidden network by name.");
                i=node(a,RISC_COMPONENT_ROW,ADD_NETWORK,"ADD NETWORK","");a->next_doc.details[i].symbol=RISC_SYMBOL_PLUS;
            }
        }
        tabs(a,0);
    }else if(a->screen==SAVED){
        copy_text(a->next_doc.routes[0].title,40,"SAVED");a->next_doc.flags|=RISC_COMPONENTS_ROOT_HEADER;
        for(unsigned k=0;k<a->saved_rows;k++){
            const char *label="OUT OF RANGE";for(unsigned j=0;j<a->scan.count;j++)if(!strcmp(a->scan.entries[j].ssid,a->saved_names[k]))label="IN RANGE";
            if(a->link_up&&!strcmp(a->connected.ssid,a->saved_names[k]))label="CONNECTED";
            i=node(a,RISC_COMPONENT_ROW,SELECT_SAVED+k,a->saved_names[k],label);a->next_doc.details[i].symbol=RISC_SYMBOL_STAR;
        }
        if(a->db_job==DB_LIST)node(a,RISC_COMPONENT_CARD,0,"LOADING SAVED NETWORKS","Reading existing profiles; nothing is overwritten.");
        else if(a->db_fault)node(a,RISC_COMPONENT_EMPTY,0,"STORAGE UNAVAILABLE","Saved profiles could not be read.");
        else if(!a->saved_rows)node(a,RISC_COMPONENT_EMPTY,0,"NO SAVED NETWORKS","Networks you successfully join are remembered here.");
        if(a->saved_base)button_node(a,SAVED_PREV,"PREVIOUS",false);
        if(a->saved_rows==8)button_node(a,SAVED_NEXT,"MORE SAVED NETWORKS",false);
        tabs(a,1);
    }else if(a->screen==DETAIL){
        copy_text(a->next_doc.routes[0].title,40,a->selected.ssid);
        bool on=a->link_up&&!strcmp(a->selected.ssid,a->connected.ssid);int r=on?a->connected_rssi:a->selected.rssi;
        copy_text(a->next_doc.subtitle,72,on?"CONNECTED / INTERNET NOT TESTED":a->selected_saved?"SAVED NETWORK":"NETWORK DETAILS");
        if(r==-127)copy_text(b,sizeof(b),"-- DBM");else snprintf(b,sizeof(b),"%d DBM",r);
        i=node(a,RISC_COMPONENT_SIGNAL_HERO,0,quality(strength(r)),b);a->next_doc.nodes[i].value=(int)strength(r);
        if(a->selected.auth==255)node(a,RISC_COMPONENT_CARD,0,"UNSUPPORTED SECURITY","Enterprise 802.1X is not supported by this provider.");
        else button_node(a,on?DISCONNECT:CONNECT,on?"DISCONNECT":a->selected_saved||!a->selected.auth?"CONNECT":"ENTER PASSWORD",!on);
        if(a->db_job==DB_FIND)stat(a,"SAVED PROFILE","LOOKING UP...");
        section(a,"DETAILS");stat(a,"SECURITY",auth_name(a->selected.auth));
        if(a->selected.channel)snprintf(b,sizeof(b),"%u / %u MHZ",a->selected.channel,a->selected.channel==14?2484:2407+a->selected.channel*5);else copy_text(b,sizeof(b),"NOT REPORTED");stat(a,"CHANNEL",b);stat(a,"BSSID","NOT REPORTED");
        if(on){ipv4_text(b,sizeof(b),a->progress.station);stat(a,"IP ADDRESS",b);ipv4_text(b,sizeof(b),a->progress.station+4);stat(a,"GATEWAY",b);ipv4_text(b,sizeof(b),a->progress.station+8);stat(a,"SUBNET MASK",b);stat(a,"DNS","NOT REPORTED");stat(a,"LINK SPEED","NOT REPORTED");stat(a,"INTERNET","NOT TESTED");
            section(a,"SIGNAL / LAST 60 S");i=node(a,RISC_COMPONENT_PLOT,0,"RSSI","DBM");a->next_doc.nodes[i].value=60;
            copy_text(a->next_doc.details[i].choices,72,"-45|-95");risc_scene_curve_v1 *cv=&a->plot.curves[0];cv->node=i+1;cv->count=60;
            for(unsigned k=0;k<60;k++)cv->points[k]=k<60-a->history_count?RISC_TELEMETRY_GAP:a->history[(a->history_pos+60-a->history_count+k-(60-a->history_count))%60];a->plot.count=1;
        }
        if(a->selected_saved){section(a,"IP SETTINGS");i=node(a,RISC_COMPONENT_SEGMENTS,INFORMATION,"","");a->next_doc.nodes[i].maximum=1;a->next_doc.nodes[i].flags=RISC_SCENE_DISABLED;copy_text(a->next_doc.details[i].choices,72,"DHCP|STATIC");stat(a,"STATIC IP","PROVIDER NOT AVAILABLE");
            i=node(a,RISC_COMPONENT_ROW,FORGET_ASK,"FORGET NETWORK","");a->next_doc.nodes[i].flags=RISC_SCENE_DESTRUCTIVE;a->next_doc.details[i].symbol=RISC_SYMBOL_DELETE;}
    }else if(a->screen==ADD){
        copy_text(a->next_doc.routes[0].title,40,"ADD NETWORK");copy_text(a->next_doc.subtitle,72,"HIDDEN OR OUT OF RANGE");
        node(a,RISC_COMPONENT_ROW,EDIT_SSID,"NETWORK NAME",a->draft.ssid[0]?a->draft.ssid:"REQUIRED");section(a,"SECURITY");
        i=node(a,RISC_COMPONENT_SEGMENTS,SECURITY,"","");a->next_doc.nodes[i].maximum=2;a->next_doc.nodes[i].value=(int)a->security;copy_text(a->next_doc.details[i].choices,72,"NONE|WPA2|WPA3");
        if(a->security)node(a,RISC_COMPONENT_ROW,EDIT_PASSWORD,"PASSWORD",a->draft.password[0]?"ENTERED / HIDDEN":"REQUIRED");
        button_node(a,SAVE_CONNECT,"SAVE & CONNECT",true);if(!portable_wifi_credentials_validate(&a->draft))a->next_doc.nodes[a->next_doc.node_count-1].flags|=RISC_SCENE_DISABLED;
        if(a->db_job)stat(a,"SAVED PROFILE","LOOKING UP...");
    }else if(a->screen==JOINING){
        copy_text(a->next_doc.routes[0].title,40,"JOINING");copy_text(a->next_doc.subtitle,72,a->draft.ssid);
        unsigned phase=a->progress.phase;
        i=node(a,RISC_COMPONENT_CONNECTION_STEP,0,"STARTING WI-FI","");a->next_doc.nodes[i].value=phase>=RISC_RADIO_JOINING?2:1;
        i=node(a,RISC_COMPONENT_CONNECTION_STEP,0,"ASSOCIATING / AUTHENTICATING","Waiting for the access point");a->next_doc.nodes[i].value=phase>=RISC_RADIO_CONNECTED?2:phase==RISC_RADIO_JOINING?1:0;
        i=node(a,RISC_COMPONENT_CONNECTION_STEP,0,"OBTAINING IP ADDRESS","");a->next_doc.nodes[i].value=phase==RISC_RADIO_CONNECTED?2:0;
        i=node(a,RISC_COMPONENT_CONNECTION_STEP,0,"INTERNET CHECK","Not exposed by this provider");a->next_doc.nodes[i].value=0;
        i=node(a,RISC_COMPONENT_PROGRESS,0,"", "Working / Cancel stops this request");a->next_doc.nodes[i].value=(int)((a->now/1000)%3);button_node(a,CANCEL,"CANCEL",false);
    }else if(a->screen==FAILURE){
        copy_text(a->next_doc.routes[0].title,40,"COULD NOT CONNECT");copy_text(a->next_doc.subtitle,72,a->selected.ssid);node(a,RISC_COMPONENT_EMPTY,0,"CONNECTION FAILED",a->error);
        if(a->selected.auth)button_node(a,PASSWORD,"RE-ENTER PASSWORD",true);button_node(a,RETRY,"TRY AGAIN",false);button_node(a,CANCEL,"BACK TO NETWORKS",false);
    }else if(a->screen==FORGET){
        a->next_doc.flags=RISC_COMPONENTS_CONFIRM;a->next_doc.cancel_action=BACK;copy_text(a->next_doc.routes[0].title,40,"CONFIRM");
        node(a,RISC_SCENE_TEXT_NODE,0,"FORGET THIS NETWORK?","You will need the password to join it again.");button_node(a,BACK,"CANCEL",false);button_node(a,FORGET_YES,"FORGET",false);a->next_doc.nodes[2].flags=RISC_SCENE_DESTRUCTIVE;
    }else{
        copy_text(a->next_doc.routes[0].title,40,"WI-FI MANAGER");node(a,RISC_COMPONENT_EMPTY,0,"UNAVAILABLE",a->message);button_node(a,BACK,"BACK",false);
    }
}

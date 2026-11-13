/* Explicit returning-Light protocol. Ticket custody stays in the adapter. */
#if !defined(PORTABLE_ALARM_CLIENT) || !defined(ALARM_SERVICE_TAGGED_V2) || !defined(PORTABLE_NATIVE_CUSTODY_FENCE)
#error Foreground alarm sleep requires the tagged native alarm client
#endif
static alarm_sleep_v1 foreground_alarm_ticket;
static bool foreground_alarm_ticket_valid;
static int foreground_alarm_terminal(void) {
 foreground_alarm_ticket_valid=false;portable_adapter_retain_silent();return -2;
}
static bool foreground_alarm_live(void) {
 return !native_custody_retained && !native_sleep_retained && !alarms.retained;
}
static int foreground_alarm_prepare_unleased(uint32_t *duration) {
 foreground_alarm_ticket_valid=false;
 if(!duration || !foreground_alarm_live())return foreground_alarm_terminal();
 *duration=0;
 if(!portable_alarm_callable(&alarms) || !alarm_service_descriptor(alarms.api)->resume_sleep)
  return foreground_alarm_terminal();
 const uint32_t snapshot_before=alarms.status.snapshot;
 for(unsigned n=0;n<64;n++) {
  alarm_sleep_v1 ticket={.struct_size=sizeof(ticket)};
  int32_t rc=alarms.api->prepare_sleep(alarms.api->context,&ticket);
  if(!portable_alarm_result(&alarms,rc) || !foreground_alarm_live())return foreground_alarm_terminal();
  if(rc==ALARM_OK) {
   /* Seconds are absolute UTC; a due/malformed plan never authorizes sleep. */
   if(ticket.struct_size!=sizeof(ticket) || !ticket.snapshot || ticket.snapshot==snapshot_before ||
      ticket.snapshot!=alarms.status.snapshot || ticket.rtc_seconds!=alarms.status.rtc_seconds ||
      (ticket.deadline && ticket.deadline<=ticket.rtc_seconds))return 0;
   if(ticket.deadline) {
    uint32_t seconds=ticket.deadline-ticket.rtc_seconds;
    /* Integer samples can be almost one second old. Wake conservatively. */
    *duration=seconds>86400u?86400000u:seconds>1u?(seconds-1u)*1000u:1u;
   }
   foreground_alarm_ticket=ticket;foreground_alarm_ticket_valid=true;return 1;
  }
  if(rc!=ALARM_PENDING)return 0;
  rc=alarms.api->step(alarms.api->context);
  if(!portable_alarm_result(&alarms,rc) || !foreground_alarm_live())return foreground_alarm_terminal();
  if(rc!=ALARM_OK && rc!=ALARM_PENDING)return 0;
  if(!portable_alarm_status(&alarms))return foreground_alarm_live()?0:foreground_alarm_terminal();
  if(!foreground_alarm_live())return foreground_alarm_terminal();
  if(alarms.status.output_uncertain)return foreground_alarm_terminal();
  if(alarms.status.state==ALARM_STATE_BLOCKED || alarms.status.state==ALARM_STATE_CUE ||
     portable_alarm_owned(&alarms) || alarms.status.occurrence.generation)return 0;
 }
 return 0;
}
static int foreground_alarm_resume_unleased(void) {
 if(!foreground_alarm_live())return foreground_alarm_terminal();
 if(!foreground_alarm_ticket_valid || !portable_alarm_callable(&alarms))return foreground_alarm_terminal();
 foreground_alarm_ticket_valid=false; /* Never replay, even on clean refusal. */
 int32_t rc=alarm_service_resume(alarms.api,&foreground_alarm_ticket);
 if(!portable_alarm_result(&alarms,rc) || !foreground_alarm_live())return foreground_alarm_terminal();
 return rc==ALARM_OK?1:0;
}
static int foreground_alarm_prepare(void *context,uint32_t *duration) {
 (void)context;
#ifdef PORTABLE_WIFI_SERVICE_LEASE
 if(!foreground_alarm_live())return foreground_alarm_terminal();
 if(!portable_wifi_services_begin())return foreground_alarm_live()?0:foreground_alarm_terminal();
#endif
 int result=foreground_alarm_prepare_unleased(duration);
#ifdef PORTABLE_WIFI_SERVICE_LEASE
 if(result<0 || !foreground_alarm_live())return foreground_alarm_terminal();
 if(!portable_wifi_services_end())return foreground_alarm_terminal();
#endif
 return result;
}
static int foreground_alarm_resume(void *context) {
 (void)context;
#ifdef PORTABLE_WIFI_SERVICE_LEASE
 if(!foreground_alarm_live())return foreground_alarm_terminal();
 if(!portable_wifi_services_begin())return foreground_alarm_terminal();
#endif
 int result=foreground_alarm_resume_unleased();
#ifdef PORTABLE_WIFI_SERVICE_LEASE
 if(result<0 || !foreground_alarm_live())return foreground_alarm_terminal();
 if(!portable_wifi_services_end())return foreground_alarm_terminal();
#endif
 return result;
}

static bool setup_owned,setup_cleanup,setup_refuse,setup_context,setup_stale;
static unsigned setup_opens,setup_polls,setup_closes,setup_releases,setup_confirms,setup_sleeps;
static uint32_t setup_state=RISC_SETUP_ADVERTISING,setup_generation=1,setup_number=123456;
static int32_t setup_open(void*c,const char*n,const risc_bluetooth_session_descriptor_v1*d,uint32_t duration,uint64_t*t){
 (void)c;assert(!setup_owned&&listeners==1&&strlen(d->password)==16&&!strcmp(d->url,fsh_url)&&!strcmp(n,"RiscRTE Files")&&duration==300000&&d->remaining_lifetime_ms<=duration);setup_owned=true;setup_opens++;*t=41;return RISC_SETUP_PENDING;
}
static int32_t setup_poll(void*c,uint64_t t,uint32_t n){(void)c;assert(setup_owned&&t==41&&n==16&&!setup_cleanup);setup_polls++;return setup_context?RISC_SETUP_CONTEXT:RISC_SETUP_OK;}
static int32_t setup_status(void*c,uint64_t t,risc_bluetooth_session_setup_status_v1*out){
 (void)c;assert(setup_owned&&t==41&&!setup_cleanup);*out=(risc_bluetooth_session_setup_status_v1){.struct_size=sizeof(*out),.state=setup_state,.pairing_generation=setup_generation,.pairing_number=setup_state==RISC_SETUP_PAIR_CONFIRM?setup_number:0,.connection_generation=setup_state>=RISC_SETUP_CONNECTED?1:0,.remaining_ms=100000,.flags=setup_state==RISC_SETUP_READY?15:0};return RISC_SETUP_OK;
}
static int32_t setup_confirm(void*c,uint64_t t,uint32_t g,uint32_t n,bool accept){
 (void)c;assert(setup_owned&&t==41&&!setup_cleanup&&g==setup_generation&&n==setup_number);setup_confirms++;
 if(setup_stale){setup_stale=false;setup_generation++;setup_number=234567;return RISC_SETUP_INVALID;}
 setup_state=accept?RISC_SETUP_READY:RISC_SETUP_ENDED;return RISC_SETUP_OK;
}
static int32_t setup_close(void*c,uint64_t t){
 (void)c;assert(setup_owned&&t==41&&listeners==1);setup_closes++;
 if(setup_refuse){setup_cleanup=true;setup_sleeps++;return RISC_SETUP_CLEANUP_PENDING;}
 setup_cleanup=false;setup_owned=false;return RISC_SETUP_OK;
}
static const risc_bluetooth_session_setup_v1 setup_api={1,sizeof(setup_api),NULL,setup_open,setup_poll,setup_status,setup_confirm,setup_close};
static bool run_setup_scene(const char*);

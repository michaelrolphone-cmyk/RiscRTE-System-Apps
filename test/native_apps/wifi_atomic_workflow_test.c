/* Actual controller against copied, scheduled atomic namespace fixture.
 * Separate Runtime tests qualify the real NativeReplicaCore publication. */
#define PORTABLE_WIFI_ATOMIC_EXPERIMENTAL
#define PORTABLE_WIFI_REMEMBER_CONNECTED
#define TEST_WIFI_ATOMIC_FIXTURE
#include "RiscKeyValueAtomicV1.h"
static risc_key_value_atomic_v1 atomic_fixture_api;
#define WIFI_WORKFLOW_LIBRARY
#include "wifi_workflow_test.c"
static uint64_t native_revision=1,native_sequence,job_token,job_revision,serial;
static unsigned submits,collects,cancels,job_state,copy_count,offered_namespace=6;
static bool job_cancelled;
static portable_wifi_atomic_cell copied[8];
static uint8_t intent_copy[160],last_intent[160];
static risc_key_value_atomic_receipt_v1 job_receipt;
static int32_t atomic_state(void*c,risc_key_value_atomic_state_v1*out){
 (void)c;if(job_state||temporary_store_busy)return RISC_ATOMIC_BUSY;
 *out=(risc_key_value_atomic_state_v1){.struct_size=sizeof(*out),.flags=3,.namespace_id=offered_namespace,.max_mutations=8,.max_value_bytes=2048,.max_group_bytes=4096,.max_intent_bytes=1024,.revision=native_revision};return 0;
}
static int32_t atomic_get(void*c,uint64_t rev,const char*k,void*out,uint32_t cap,uint32_t*n){
 if(job_state||temporary_store_busy)return RISC_ATOMIC_BUSY;
 if(rev!=native_revision)return RISC_ATOMIC_STALE;
 int rc=fake_get(c,k,out,cap,n);return rc==0?0:rc==RISC_KEY_VALUE_NOT_FOUND?RISC_ATOMIC_NOT_FOUND:RISC_ATOMIC_IO;
}
static int32_t atomic_submit(void*c,uint64_t rev,const risc_key_value_atomic_mutation_v1*m,uint32_t count,const void*intent,uint32_t n,uint64_t*token){
 (void)c;*token=0;if(job_state||temporary_store_busy)return RISC_ATOMIC_BUSY;
 if(rev!=native_revision)return RISC_ATOMIC_STALE;
 assert(count<=8&&n==160);copy_count=count;for(unsigned i=0;i<count;i++){assert(m[i].kind==RISC_ATOMIC_PUT&&m[i].size<=64);strcpy(copied[i].key,m[i].key);copied[i].size=m[i].size;memcpy(copied[i].value,m[i].value,m[i].size);}
 memcpy(intent_copy,intent,n);job_revision=rev;job_token=++serial;*token=job_token;job_state=1;job_cancelled=false;++submits;return 0;
}
static int32_t atomic_collect(void*c,uint64_t token,risc_key_value_atomic_receipt_v1*out){
 (void)c;assert(token==job_token&&job_state);if(job_state!=2)return RISC_ATOMIC_PENDING;
 *out=job_receipt;job_state=0;job_token=0;++collects;return 0;
}
static int32_t atomic_cancel(void*c,uint64_t token){(void)c;assert(token==job_token&&job_state);job_cancelled=true;++cancels;return RISC_ATOMIC_PENDING;}
static void native_worker(void){
 assert(job_state==1);int rc=job_cancelled?RISC_ATOMIC_CANCELLED:job_revision!=native_revision?RISC_ATOMIC_STALE:0;
 if(!rc){for(unsigned i=0;i<copy_count;i++)assert(!fake_put(NULL,copied[i].key,copied[i].value,copied[i].size));memcpy(last_intent,intent_copy,160);++native_revision;++native_sequence;}
 job_receipt=(risc_key_value_atomic_receipt_v1){.struct_size=sizeof(job_receipt),.status=rc,.revision=native_revision,.operation_id=rc?0:native_sequence};
 portable_wifi_credentials_wipe(copied,sizeof(copied));portable_wifi_credentials_wipe(intent_copy,sizeof(intent_copy));job_state=2;
}
static void prepared(void){for(unsigned n=0;profile_work&&!job_state;n++){wifi_profiles_step();assert(n<300);}}
static void drained(void){for(unsigned n=0;profile_work||remember_pending;n++){wifi_profiles_step();if(job_state==1)native_worker();assert(n<300);}}
static void set_record(unsigned slot,const char*ssid,const char*password){portable_wifi_credentials v={{0},{0}};strcpy(v.ssid,ssid);strcpy(v.password,password);assert(!portable_wifi_profile_save(&kv_api,slot,&v));++native_revision;}
static void assert_record(unsigned slot,const char*ssid,const char*password){portable_wifi_credentials v={{0},{0}};assert(!portable_wifi_profile_load(&kv_api,slot,&v));assert(!strcmp(v.ssid,ssid)&&!strcmp(v.password,password));}
static void clean_edit(void){for(unsigned i=0;i<sizeof(wifi_atomic_edit.intent);i++)assert(!wifi_atomic_edit.intent[i]);for(unsigned i=0;i<sizeof(wifi_atomic_edit.mutations);i++)assert(!((uint8_t*)wifi_atomic_edit.mutations)[i]);assert(!wifi_atomic_edit.expected.password[0]&&!wifi_atomic_edit.desired.password[0]);}
int main(int argc,char**argv){
 (void)text_done;(void)seed;(void)search_done;(void)stopped;
 assert(argc==2);unsigned which=(unsigned)atoi(argv[1]);
 atomic_fixture_api=(risc_key_value_atomic_v1){1,sizeof(atomic_fixture_api),NULL,atomic_state,atomic_get,atomic_submit,atomic_collect,atomic_cancel};
 if(which==12)kv_denied=1;
 workflow_start(which>=19&&which<=21);draft();
 if(which==0){wifi_activate(6);prepared();assert(job_state==1&&!writes&&!native_sequence);clean_edit();native_worker();assert(native_sequence==1&&profile_work);drained();assert(saved_state==0&&!draft_dirty);assert_record(0,"Fixture network","testpass123");assert(last_intent[8]==1&&!last_intent[9]);}
 else if(which==1){set_record(0,"Fixture network","testpass123");unsigned before=writes;wifi_activate(7);prepared();assert(page==WP_FORGET&&writes==before&&!submits);wifi_activate(1);drained();portable_wifi_credentials out;assert(portable_wifi_profile_load(&kv_api,0,&out)==PORTABLE_WIFI_CREDENTIALS_EMPTY);assert(last_intent[8]==2&&last_intent[9]);for(unsigned i=49;i<113;i++)assert(!last_intent[i]);}
 else if(which==2){set_record(0,"Fixture network","testpass123");wifi_activate(7);prepared();set_record(0,"Replacement","replacement-pass");unsigned before=writes;wifi_activate(1);drained();assert(!native_sequence&&writes==before&&strstr(wifi_message,"changed"));assert_record(0,"Replacement","replacement-pass");}
 else if(which==3){set_record(0,"Fixture network","testpass123");set_record(1,"Fixture network","other-pass");unsigned before=writes;wifi_activate(6);drained();assert(writes==before&&!submits&&strstr(wifi_message,"conflict"));clean_edit();}
 else if(which==4){temporary_store_busy=true;wifi_activate(6);for(unsigned i=0;i<50;i++)wifi_profiles_step();assert(profile_work&&!writes&&!submits);temporary_store_busy=false;drained();assert(native_sequence==1);}
 else if(which==5){wifi_activate(6);wifi_profiles_step();++native_revision;drained();assert(!writes&&!submits&&strstr(wifi_message,"changed"));clean_edit();}
 else if(which==6){set_record(0,"Fixture network","testpass123");wifi_activate(6);drained();assert(native_sequence==1&&last_intent[9]);assert_record(0,"Fixture network","testpass123");}
 else if(which==7){wifi_activate(6);wifi_profiles_step();assert(!wifi_back());assert(!profile_work&&!submits);clean_edit();}
 else if(which==8){wifi_activate(6);prepared();assert(job_state==1);assert(!wifi_back()&&profile_work&&job_state);native_worker();drained();assert(!writes&&!native_sequence&&collects==1&&cancels==1);clean_edit();}
 else if(which==9){wifi_activate(6);prepared();native_worker();assert(!wifi_back());assert(!profile_work&&collects==1&&native_sequence==1&&saved_state==0);assert_record(0,"Fixture network","testpass123");}
 else if(which==10){wifi_connect();prepared();assert(connects==1&&joining&&!writes&&!submits&&wifi_atomic_connect_ready);fake_link=WIFI_LINK_UP;tick(250);assert(remember_pending&&!writes);drained();assert(native_sequence==1);assert_record(0,"Fixture network","testpass123");}
 else if(which==11){wifi_connect();prepared();set_record(0,"Fixture network","other-pass");unsigned before=writes;fake_link=WIFI_LINK_UP;tick(250);drained();assert(!native_sequence&&writes==before&&strstr(wifi_message,"changed"));assert_record(0,"Fixture network","other-pass");}
 else if(which==12){assert(!wifi_store);wifi_activate(6);assert(!profile_work&&!writes&&!submits);wifi_connect();assert(!connects&&!writes&&!submits);}
 else if(which==13){fail_connect=1;wifi_connect();prepared();assert(connects==1&&!joining&&!submits&&!writes);assert(wifi_back());assert(!submits);}
 else if(which==14){offered_namespace=1;wifi_activate(6);drained();assert(!submits&&!writes);}
 else if(which==15){set_record(0,"Other AP","other-pass");unsigned before=writes;wifi_activate(6);prepared();assert(copy_count==4&&writes==before);native_worker();drained();uint32_t count=0;assert(!portable_wifi_profile_count(&kv_api,&count)&&count==2);assert_record(1,"Fixture network","testpass123");}
 else if(which==16){wifi_activate(6);prepared();assert(!portable_wifi_suspend()&&job_state);native_worker();assert(portable_wifi_suspend()&&!profile_work&&collects==1);}
 else if(which==17){set_record(0,"Fixture network","testpass123");wifi_activate(7);prepared();assert(!wifi_back());assert(!submits&&!forget_index_valid);clean_edit();}
 else if(which==18){wifi_connect();prepared();assert(remember_join);assert(wifi_disconnect());assert(!submits&&!writes);wifi_activate(6);drained();assert(native_sequence==1);}
 else if(which==19){wifi_connect();prepared();assert(async_starts==1&&joining&&!writes&&!submits);progress.phase=RISC_RADIO_CONNECTED;progress.station_state=2;tick(250);assert(remember_pending);drained();assert(native_sequence==1&&service_begins&&service_begins==service_ends&&!service_live);assert(!wifi_disconnect());stopped();}
 else if(which==20){begin_unavailable=true;wifi_connect();prepared();assert(!wifi_operation&&!async_starts&&!submits&&!writes&&!remember_join);assert(wifi_back());}
 else if(which==21){wifi_connect();wifi_profiles_step();assert(!wifi_back()&&!profile_work&&!async_starts&&!writes);clean_edit();}
 else if(which==22){wifi_activate(6);prepared();native_worker();job_receipt.operation_id=0;wifi_profiles_step();assert(wifi_atomic_edit.status==RISC_ATOMIC_RETAINED&&portable_wifi_atomic_pending(&wifi_atomic_edit)&&cleanup_pending);puts("Atomic Wi-Fi malformed receipt retains invocation PASS");return 0;}
 else assert(!"unknown scenario");
 if(opened){if(remember_join)assert(wifi_disconnect());if(job_state==1)native_worker();drained();}
 finish();clean_edit();printf("Atomic Wi-Fi controller case %u PASS\n",which);
}

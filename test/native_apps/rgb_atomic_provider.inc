/* Copied transactional provider double; no fixture legacy writer authority. */
static uint64_t rgb_revision=7,rgb_token;
static unsigned rgb_count,rgb_submits,rgb_automatic,rgb_user,rgb_collects;
static char rgb_keys[2][16];static uint8_t rgb_values[2][64];static uint32_t rgb_sizes[2];
static int32_t rgb_state(void*c,risc_key_value_atomic_state_v1*s){(void)c;*s=(risc_key_value_atomic_state_v1){sizeof(*s),3,1,16,64,4096,1024,0,rgb_revision};return 0;}
static int32_t rgb_get(void*c,uint64_t rev,const char*k,void*out,uint32_t cap,uint32_t*size){
 *size=0;if(rev!=rgb_revision)return RISC_ATOMIC_STALE;
 uint8_t data[64];uint32_t n=0;int rc=quick_get(c,k,data,sizeof(data),&n);
 if(rc==RISC_KEY_VALUE_NOT_FOUND)return RISC_ATOMIC_NOT_FOUND;
 if(rc)return RISC_ATOMIC_IO;
 *size=n;if(cap<n)return RISC_ATOMIC_BUFFER_SMALL;memcpy(out,data,n);return 0;
}
static int32_t rgb_submit(void*c,uint64_t rev,const risc_key_value_atomic_mutation_v1*m,uint32_t count,const void*intent,uint32_t bytes,uint64_t*t){
 (void)c;*t=0;assert(fixture_foreground_frame_safe()&&!rgb_token&&count&&count<=2&&bytes==176);
 const uint8_t*p=intent;assert(((!memcmp(p,"PFI0001",8)&&p[8]==PORTABLE_PREFERENCE_USER)||(!memcmp(p,"PFA0001",8)&&p[8]==PORTABLE_PREFERENCE_AUTOMATIC))&&!strcmp((const char*)p+12,m[0].key));
 if(rev!=rgb_revision)return RISC_ATOMIC_STALE;
 if(p[8]==PORTABLE_PREFERENCE_USER)rgb_user++;else rgb_automatic++;
 for(unsigned i=0;i<count;i++){assert(m[i].kind==RISC_ATOMIC_PUT&&strlen(m[i].key)<16&&m[i].size<=64);strcpy(rgb_keys[i],m[i].key);memcpy(rgb_values[i],m[i].value,m[i].size);rgb_sizes[i]=m[i].size;}
 rgb_count=count;rgb_collects=0;*t=rgb_token=++rgb_submits;return 0;
}
static int32_t rgb_collect(void*c,uint64_t t,risc_key_value_atomic_receipt_v1*r){
 assert(t&&t==rgb_token&&fixture_foreground_frame_safe());if(++rgb_collects<2)return RISC_ATOMIC_PENDING;
 int rc=0;
 for(unsigned i=0;i<rgb_count;i++)if((pref_index(rgb_keys[i])>=0&&pref_write_fail)||(low_key(rgb_keys[i])==low_write_fail&&low_write_fail>=0))rc=RISC_ATOMIC_IO;
 rgb_publishing=true;
 if(!rc)for(unsigned i=0;i<rgb_count;i++)assert(!quick_put(c,rgb_keys[i],rgb_values[i],rgb_sizes[i]));
 rgb_publishing=false;if(!rc)rgb_revision++;
 *r=(risc_key_value_atomic_receipt_v1){sizeof(*r),rc,rgb_revision,rc?0:rgb_revision};rgb_token=0;return 0;
}
static int32_t rgb_cancel(void*c,uint64_t t){(void)c;assert(t==rgb_token);return 0;}
static const risc_key_value_atomic_v1 rgb_api={1,sizeof(rgb_api),NULL,rgb_state,rgb_get,rgb_submit,rgb_collect,rgb_cancel};

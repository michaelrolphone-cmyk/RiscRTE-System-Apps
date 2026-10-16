#include "CrashReportFiles.h"
#include <assert.h>
#include <stdio.h>

#define FILES 8u
#define EVENTS 160u
static const char sha[]="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
static unsigned char text[CRASH_REPORT_FILES_TEXT_MAX];
typedef enum {STAT,MKDIR,OPEN,WRITE,READ,INFO,SYNC,CLOSE,RENAME,REMOVE,KINDS} kind;
typedef struct {bool used,synced;char path[128];unsigned char data[4097];size_t size;} file;
typedef struct {
 risc_storage_files_v1 api;file files[FILES];bool directory,forbid,unsafe,readonly,unavailable,busy;
 unsigned calls,counts[KINDS],fail_call,retained_call,short_call,corrupt_call,unsafe_at,safe_calls,duration;
 unsigned handle,slot,closes,removes,rename_collision;uint32_t flags;size_t position;uint64_t now;
 bool rename_commit_failure,remove_refused;kind events[EVENTS];
} fake;
static unsigned find_file(fake *f,const char *path){for(unsigned i=0;i<FILES;++i)if(f->files[i].used&&!strcmp(f->files[i].path,path))return i;return FILES;}
static unsigned add_file(fake *f,const char *path,const void *bytes,size_t size){
 assert(find_file(f,path)==FILES&&size<=4097);
 for(unsigned i=0;i<FILES;++i)if(!f->files[i].used){file *v=&f->files[i];memset(v,0,sizeof(*v));v->used=true;strcpy(v->path,path);v->size=size;if(size)memcpy(v->data,bytes,size);return i;}
 assert(!"full test backend");return FILES;
}
static int32_t event(fake *f,kind op){
 assert(!f->forbid&&!f->unsafe&&f->calls<EVENTS);f->events[f->calls]=op;++f->calls;++f->counts[op];f->now+=f->duration;
 if(f->calls==f->retained_call){f->forbid=true;return RISC_FILES_RETAINED;}
 if(f->calls==f->fail_call)return RISC_FILES_IO;
 return f->unavailable?RISC_FILES_UNAVAILABLE:f->busy?RISC_FILES_BUSY:RISC_FILES_OK;
}
static int32_t stat_file(void *c,const char *path,risc_storage_files_stat_v1 *out){
 fake *f=c;int32_t r=event(f,STAT);if(r)return r;assert(out->struct_size>=sizeof(*out));
 *out=(risc_storage_files_stat_v1){.struct_size=sizeof(*out)};
 if(!strcmp(path,"/CrashReports")){out->flags=RISC_FILES_DIRECTORY;return f->directory?RISC_FILES_OK:RISC_FILES_NOT_FOUND;}
 unsigned i=find_file(f,path);if(i==FILES)return RISC_FILES_NOT_FOUND;out->size=f->files[i].size;return RISC_FILES_OK;
}
static int32_t mkdir_file(void *c,const char *path){
 fake *f=c;int32_t r=event(f,MKDIR);if(r)return r;assert(!strcmp(path,"/CrashReports"));
 if(f->readonly)return RISC_FILES_DENIED;
 if(f->directory)return RISC_FILES_EXISTS;
 f->directory=true;return RISC_FILES_OK;
}
static int32_t open_file(void *c,const char *path,uint32_t flags,uint32_t *out){
 fake *f=c;*out=0;assert(!f->handle);int32_t r=event(f,OPEN);if(r)return r;
 unsigned i=find_file(f,path);
 if(flags&RISC_FILES_WRITE){
  assert(flags==(RISC_FILES_WRITE|RISC_FILES_CREATE|RISC_FILES_EXCLUSIVE)&&strstr(path,".tmp")&&!strstr(path,".txt"));
  if(f->readonly)return RISC_FILES_DENIED;
  if(i!=FILES)return RISC_FILES_EXISTS;
  i=add_file(f,path,NULL,0);
 }else{assert(flags==RISC_FILES_READ);if(i==FILES)return RISC_FILES_NOT_FOUND;}
 f->handle=17;f->slot=i;f->position=0;f->flags=flags;*out=17;return RISC_FILES_OK;
}
static int32_t write_file(void *c,uint32_t h,const void *bytes,uint32_t size,uint32_t *out){
 fake *f=c;assert(h==f->handle&&(f->flags&RISC_FILES_WRITE)&&size&&size<=512);*out=0;
 int32_t r=event(f,WRITE);if(r)return r;if(f->calls==f->short_call)--size;
 assert(f->position+size<=4096);file *v=&f->files[f->slot];memcpy(v->data+f->position,bytes,size);
 f->position+=size;v->size=f->position;v->synced=false;*out=size;return RISC_FILES_OK;
}
static int32_t read_file(void *c,uint32_t h,void *bytes,uint32_t size,uint32_t *out){
 fake *f=c;assert(h==f->handle&&(f->flags&RISC_FILES_READ)&&size&&size<=512);*out=0;
 int32_t r=event(f,READ);if(r)return r;file *v=&f->files[f->slot];if(size>v->size-f->position)size=(uint32_t)(v->size-f->position);
 if(f->calls==f->short_call&&size)--size;
 if(size)memcpy(bytes,v->data+f->position,size);
 if(f->calls==f->corrupt_call&&size)((unsigned char*)bytes)[0]^=0x80;
 f->position+=size;*out=size;return RISC_FILES_OK;
}
static int32_t info_file(void *c,uint32_t h,uint64_t *size,uint64_t *position){fake *f=c;assert(h==f->handle);int32_t r=event(f,INFO);if(r)return r;*size=f->files[f->slot].size;*position=f->position;return RISC_FILES_OK;}
static int32_t sync_file(void *c,uint32_t h){fake *f=c;assert(h==f->handle);int32_t r=event(f,SYNC);if(r)return r;f->files[f->slot].synced=true;return RISC_FILES_OK;}
static int32_t close_file(void *c,uint32_t h){fake *f=c;assert(h==f->handle);++f->closes;int32_t r=event(f,CLOSE);if(r){f->forbid=true;return r;}f->handle=0;return RISC_FILES_OK;}
static int32_t remove_file(void *c,const char *path){
 fake *f=c;assert(!f->handle&&strstr(path,".tmp")&&!strstr(path,".txt"));++f->removes;int32_t r=event(f,REMOVE);if(r)return r;
 if(f->remove_refused||f->readonly)return RISC_FILES_DENIED;
 unsigned i=find_file(f,path);if(i==FILES)return RISC_FILES_NOT_FOUND;f->files[i].used=false;return RISC_FILES_OK;
}
static int32_t rename_file(void *c,const char *from,const char *to){
 fake *f=c;assert(!f->handle&&strstr(from,".tmp")&&strstr(to,".txt"));int32_t r=event(f,RENAME);if(r)return r;
 if(f->counts[RENAME]==f->rename_collision)add_file(f,to,"other",5);
 if(find_file(f,to)!=FILES)return RISC_FILES_EXISTS;
 unsigned i=find_file(f,from);assert(i<FILES&&f->files[i].synced);strcpy(f->files[i].path,to);
 return f->rename_commit_failure?RISC_FILES_IO:RISC_FILES_OK;
}
static bool safe(void *c){fake *f=c;if(++f->safe_calls==f->unsafe_at)f->unsafe=true;return !f->unsafe;}
static uint64_t now(void *c){return ((fake*)c)->now;}
static void init(fake *f){
 memset(f,0,sizeof(*f));f->api=(risc_storage_files_v1){.api_version=1,.struct_size=sizeof(f->api),.context=f,
 .open=open_file,.close=close_file,.read=read_file,.write=write_file,.info=info_file,.sync=sync_file,.stat=stat_file,.mkdir=mkdir_file,.rename=rename_file,.remove=remove_file};
}
static crash_report_files_result run(fake *f,crash_report_files_state *s,size_t size,uint32_t budget){crash_report_files_hooks h={f,now,safe,budget};return crash_report_files_export(s,&f->api,sha,7,81,text,size,&h);}
static void paths(char *final,char *temporary,size_t size){uint32_t crc=crash_report_files_crc32(text,size);assert(crash_report_files_path(final,128,sha,7,81,crc,false));assert(crash_report_files_path(temporary,128,sha,7,81,crc,true));}
static void assert_saved(fake *f,size_t size){char final[128],tmp[128];paths(final,tmp,size);unsigned i=find_file(f,final);assert(i<FILES&&f->files[i].size==size&&!memcmp(f->files[i].data,text,size)&&f->files[i].synced&&!f->handle);}
static void assert_retained(fake *f,crash_report_files_state *s){unsigned calls=f->calls,hooks=f->safe_calls;assert(s->retained&&run(f,s,4096,0)==CRASH_REPORT_FILES_RETAINED&&f->calls==calls&&f->safe_calls==hooks);}
static void normal_and_dedupe(void){
 const size_t sizes[]={1,511,512,513,4095,4096};
 for(unsigned i=0;i<sizeof(sizes)/sizeof(sizes[0]);++i){fake f;init(&f);crash_report_files_state s={0};assert(run(&f,&s,sizes[i],0)==CRASH_REPORT_FILES_SAVED);assert_saved(&f,sizes[i]);assert(f.calls==s.operations&&s.operations<=96);unsigned writes=f.counts[WRITE],renames=f.counts[RENAME];assert(run(&f,&s,sizes[i],0)==CRASH_REPORT_FILES_SAVED);assert_saved(&f,sizes[i]);assert(writes==f.counts[WRITE]&&renames==f.counts[RENAME]);}
}
static void unavailable_missing_errors(void){
 fake f;crash_report_files_state s={0};init(&f);f.unavailable=true;assert(run(&f,&s,4096,0)==CRASH_REPORT_FILES_DEFERRED&&f.calls==1&&!f.counts[MKDIR]);
 init(&f);f.busy=true;assert(run(&f,&s,4096,0)==CRASH_REPORT_FILES_DEFERRED&&f.calls==1);
 init(&f);f.fail_call=1;assert(run(&f,&s,4096,0)==CRASH_REPORT_FILES_DEFERRED&&f.calls==1&&!f.counts[MKDIR]);
 init(&f);f.readonly=true;assert(run(&f,&s,4096,0)==CRASH_REPORT_FILES_DEFERRED&&!f.handle&&!f.counts[WRITE]);
 init(&f);f.directory=true;f.readonly=true;assert(run(&f,&s,4096,0)==CRASH_REPORT_FILES_DEFERRED&&!f.handle&&!f.counts[WRITE]);
 init(&f);assert(run(&f,&s,0,0)==CRASH_REPORT_FILES_DEFERRED&&!f.calls);assert(run(&f,&s,4097,0)==CRASH_REPORT_FILES_DEFERRED&&!f.calls);
 f.api.struct_size=8;assert(run(&f,&s,1,0)==CRASH_REPORT_FILES_DEFERRED&&!f.calls);f.api.struct_size=sizeof(f.api);f.api.rename=NULL;assert(run(&f,&s,1,0)==CRASH_REPORT_FILES_DEFERRED&&!f.calls);
}
static void identities_collisions_retry(void){
 assert(crash_report_files_crc32("123456789",9)==UINT32_C(0xcbf43926));char final[128],tmp[128],other[128],upper[65];paths(final,tmp,4096);
 assert(strstr(final,"/CrashReports/")==final&&strstr(final,"-00000007-00000051-")&&strlen(final)<128);
 for(unsigned i=0;i<65;++i)upper[i]=sha[i]>='a'&&sha[i]<='f'?(char)(sha[i]-32):sha[i];
 assert(crash_report_files_path(other,128,upper,7,81,crash_report_files_crc32(text,4096),false)&&!strcmp(other,final));upper[63]='g';assert(!crash_report_files_path(other,128,upper,7,81,0,false));assert(!crash_report_files_path(other,100,sha,7,81,0,false));
 fake f;crash_report_files_state s={0};init(&f);f.directory=true;unsigned i=add_file(&f,final,text,4096);f.files[i].data[0]^=1;
 assert(run(&f,&s,4096,0)==CRASH_REPORT_FILES_DEFERRED&&!f.counts[WRITE]&&!f.removes&&!f.handle);
 init(&f);f.directory=true;add_file(&f,final,"short",5);assert(run(&f,&s,4096,0)==CRASH_REPORT_FILES_DEFERRED&&!f.counts[OPEN]);
 init(&f);f.rename_collision=1;assert(run(&f,&s,4096,0)==CRASH_REPORT_FILES_DEFERRED&&!f.handle&&f.removes==1);i=find_file(&f,final);assert(i<FILES&&f.files[i].size==5&&!memcmp(f.files[i].data,"other",5));
 init(&f);f.directory=true;add_file(&f,tmp,"partial",7);assert(run(&f,&s,4096,0)==CRASH_REPORT_FILES_SAVED&&f.removes==1);assert_saved(&f,4096);
 init(&f);f.rename_commit_failure=true;assert(run(&f,&s,4096,0)==CRASH_REPORT_FILES_DEFERRED&&!f.handle);f.rename_commit_failure=false;assert(run(&f,&s,4096,0)==CRASH_REPORT_FILES_SAVED);assert_saved(&f,4096);
}
static void faults_and_bounds(void){
 fake baseline;init(&baseline);crash_report_files_state state={0};assert(run(&baseline,&state,4096,0)==CRASH_REPORT_FILES_SAVED);
 for(unsigned call=1;call<=baseline.calls;++call){
  fake f;init(&f);crash_report_files_state s={0};f.fail_call=call;int result=run(&f,&s,4096,0);assert(result!=CRASH_REPORT_FILES_SAVED);assert(f.calls==s.operations&&f.calls<=96);
  if(baseline.events[call-1]==CLOSE){assert(result==CRASH_REPORT_FILES_RETAINED);assert_retained(&f,&s);}else assert(result==CRASH_REPORT_FILES_DEFERRED&&!f.handle);
  init(&f);s=(crash_report_files_state){0};f.retained_call=call;assert(run(&f,&s,4096,0)==CRASH_REPORT_FILES_RETAINED&&f.calls==call);assert_retained(&f,&s);
  if(baseline.events[call-1]==WRITE||baseline.events[call-1]==READ){
   init(&f);s=(crash_report_files_state){0};f.short_call=call;result=run(&f,&s,4096,0);
   /* EOF reads stay zero, so shortening those is intentionally a no-op. */
   if(result==CRASH_REPORT_FILES_SAVED)assert_saved(&f,4096);else assert(result==CRASH_REPORT_FILES_DEFERRED&&!f.handle);
   if(baseline.events[call-1]==READ){init(&f);s=(crash_report_files_state){0};f.corrupt_call=call;result=run(&f,&s,4096,0);if(result==CRASH_REPORT_FILES_SAVED)assert_saved(&f,4096);else assert(result==CRASH_REPORT_FILES_DEFERRED&&!f.handle);}
  }
 }
 for(unsigned hook=1;hook<=baseline.safe_calls;++hook){fake f;init(&f);crash_report_files_state s={0};f.unsafe_at=hook;assert(run(&f,&s,4096,0)==CRASH_REPORT_FILES_RETAINED);assert_retained(&f,&s);}
 for(unsigned budget=1;budget<=baseline.calls+1;++budget){fake f;init(&f);crash_report_files_state s={0};f.duration=1;int result=run(&f,&s,4096,budget);assert(result!=CRASH_REPORT_FILES_RETAINED&&!f.handle&&f.calls<=96);if(result==CRASH_REPORT_FILES_SAVED)assert_saved(&f,4096);else assert(f.calls<=budget+CRASH_REPORT_FILES_CLEANUP_RESERVE);}
 // A refused cleanup is tried once, never turned into success or retried.
 for(unsigned call=1;call<=baseline.calls;++call)if(baseline.events[call-1]==WRITE){fake f;init(&f);crash_report_files_state s={0};f.short_call=call;f.remove_refused=true;assert(run(&f,&s,4096,0)==CRASH_REPORT_FILES_DEFERRED&&!f.handle&&f.removes==1);break;}
}
int main(void){for(unsigned i=0;i<sizeof(text);++i)text[i]=(unsigned char)(i*37u+11u);normal_and_dedupe();unavailable_missing_errors();identities_collisions_retry();faults_and_bounds();puts("PASS: generic crash files, exact identity/text/dedupe, authoritative missing vs unavailable/IO, collisions, retry, every-call failures/retention, deadlines, unsafe hooks, one-shot cleanup");return 0;}

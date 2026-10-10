/* Actual selected sparse Home and product sleep hook. Runtime/native power
 * callbacks remain the strict existing host doubles; see separate ELF matrix. */
#define TEST_SHARED_QUICK_REFERENCE
#define SPARSE_FIXTURE_MAIN shared_reference_base_main
#include "sparse_clock_startup_test.c"
#include "RiscResidentShellV1.h"
extern void reference_observe_ui(reference_ui*);
static bool reference_opened,reference_ready;
static uint32_t reference_start,reference_open_at;
static bool reference_sparse_ready(void){
 if(!getenv("REFERENCE_DRAWER"))return true;
 if(reference_ready)return true;
 reference_ui u;reference_observe_ui(&u);
 if(u.modal&&!reference_opened){reference_opened=true;reference_open_at=ms;}
 if(reference_opened&&u.modal&&u.position==240u*256u&&!u.neutral&&ms-reference_open_at>=400)reference_ready=true;
 return reference_ready;
}
static bool reference_sparse_touch(void*c,risc_touch_snapshot_v1*out){
 (void)c;safe();*out=(risc_touch_snapshot_v1){.width=480,.height=800};
 if(!getenv("REFERENCE_DRAWER")||reference_ready)return true;
 if(!reference_start)reference_start=ms+1;
 unsigned at=ms>=reference_start?ms-reference_start:0;
 if(at>=80&&at<180){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=240,.y=(uint16_t)(at<120?20:160)};}
 return true;
}
static int32_t ref_register(uint64_t id,const risc_resident_callbacks_v1*c){(void)id;(void)c;assert(!"Crown-only Home fixture must not launch a child");return RISC_RESIDENT_DENIED;}
static int32_t ref_run(uint64_t id,const char*p,risc_resident_result_v1*out){(void)id;(void)p;(void)out;assert(!"Crown-only Home fixture must not launch a child");return RISC_RESIDENT_DENIED;}
static int32_t ref_exit(uint64_t id){(void)id;assert(!"Crown-only Home fixture must not launch a child");return RISC_RESIDENT_DENIED;}
static bool ref_resident(risc_resident_client_v1*out){*out=(risc_resident_client_v1){.api_version=1,.struct_size=sizeof(*out),.invocation=1,.role=RISC_RESIDENT_ROLE_HOST,.register_shell=ref_register,.run_foreground=ref_run,.request_foreground_exit=ref_exit};return true;}
#ifndef REFERENCE_HOME_MAIN
#define REFERENCE_HOME_MAIN main
#endif
int REFERENCE_HOME_MAIN(int argc,char**argv){
 runtime.resident_shell=ref_resident;
 int result=shared_reference_base_main(argc,argv);assert(!result);
 assert(lock_attempted&&entries==1&&!launches&&lock_clean_frames);
 assert(reference_opened==(getenv("REFERENCE_DRAWER")!=NULL));
 if(reference_opened)assert(reference_ready);
 printf("Actual sparse Home crown: drawer=%s direction=%s desk-clean=%u terminal-entry=%u PASS\n",reference_opened?"open":"closed",getenv("DESK_DIRECTION"),lock_clean_frames,entries);return 0;
}

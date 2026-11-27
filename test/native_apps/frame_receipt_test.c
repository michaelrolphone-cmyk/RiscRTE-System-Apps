#include <assert.h>
#include <stdio.h>
#define PORTABLE_FRAME_RECEIPTS
#define PORTABLE_RASTER_SNAPSHOT
#define PORTABLE_NATIVE_CUSTODY_FENCE
#include "../../lib/PortableApps/src/adapter.c"
const t5_app_manifest_t portable_catalog[]={{.compatible=false}};
const unsigned portable_catalog_count=0;
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){(void)v;return NULL;}
int main(void){
 failed=false;frame_requested_id=5;frame_replay_id=5;frame_completed_id=4;
 raster_sealed=true;assert(!portable_paper_frame_idle());raster_discard();
 assert(portable_paper_frame_idle()&&portable_frame_requested_id()==5&&portable_frame_completed_id()==4);
 frame_receipt_token=7;frame_token_id=5;frame_receipt_complete(8);assert(frame_completed_id==4);
 frame_receipt_complete(7);assert(frame_completed_id==5&&!frame_receipt_token&&!frame_token_id);
 /* An unrelated modal completion cannot impersonate the captured app frame. */
 frame_receipt_token=8;frame_token_id=0;frame_receipt_complete(8);assert(frame_completed_id==5);
 frame_requested_id=6;frame_replay_id=6;raster_sealed=true;raster_discard();assert(frame_completed_id==5);
 frame_receipt_token=9;frame_token_id=7;frame_receipt_complete(9);assert(frame_completed_id==7&&frame_completed_id!=6);
 failed=true;assert(!portable_paper_frame_idle()&&!portable_frame_requested_id()&&!portable_frame_completed_id());
 frame_receipt_token=10;frame_token_id=8;frame_receipt_complete(10);assert(frame_completed_id==7);
 failed=false;frame_requested_id=UINT64_MAX;present(false);assert(failed&&frame_requested_id==UINT64_MAX);
 puts("Frame receipts: exact token, discard, unrelated completion, failure and exhaustion PASS");
}

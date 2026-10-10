/* Exact selected adapter, with only read-only observations and orderly exit. */
#include CLOCK_ADAPTER_SOURCE
void clock_gt_finish(void){handoff_requested=true;}
bool clock_gt_frame_pending(void){return paper_token!=0||raster_sealed||surface.frame!=0;}

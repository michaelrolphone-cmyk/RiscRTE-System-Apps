/* Copied native evidence is shown only by a healthy foreground host. This is
 * deliberately not a panic renderer and never runs after a custody fence. */
#ifdef RISC_RUNTIME_FAILURE_EVIDENCE_V1_SIZE
#include "RiscFailureEvidenceV1.h"
typedef struct {unsigned line,page,rows;bool paint;} failure_evidence_layout;
static void failure_evidence_line(failure_evidence_layout *layout,const char *line) {
 if(layout->paint && layout->line/layout->rows==layout->page)
  pp_text(24,96+(int)(layout->line%layout->rows)*22,width()-48,line,1|PAPER_TEXT_LITERAL,false,true);
 ++layout->line;
}
/* Pixel-measured, bounded wrapping preserves every copied byte on additional
 * pages. Control/non-ASCII bytes are rendered as spaces, never font commands. */
static void failure_evidence_text(failure_evidence_layout *layout,const char *text) {
 char line[96];unsigned used=0;
 for(unsigned i=0;i<193 && text[i];++i) {
  unsigned char c=(unsigned char)text[i];if(c<32 || c>126)c=' ';
  if(!used && c==' ')continue;
  line[used]=(char)c;line[used+1]=0;
  if(used && (used==sizeof(line)-2 || pp_measure(line,false)>width()-48)) {
   unsigned split=used;while(split && line[split-1]!=' ')--split;
   unsigned count=split?split-1:used;line[count]=0;failure_evidence_line(layout,line);
   unsigned tail=split?used-split:0;if(tail)memmove(line,line+split,tail);
   used=tail;line[used]=(char)c;
  }
  if(!used && c==' ')continue;
  ++used;
 }
 if(used){line[used]=0;failure_evidence_line(layout,line);}
}
#include "FailureEvidenceReport.h"
#include "failure_archive.inc"
static bool failure_evidence_render_line(void *context,const char *line) {
 failure_evidence_text((failure_evidence_layout *)context,line);return true;
}
static unsigned failure_evidence_content(const risc_failure_evidence_v1 *record,unsigned page,bool paint) {
 failure_evidence_layout layout={0,page,(unsigned)(height()-210)/22,paint};
 if(!layout.rows)layout.rows=1;
 if(!failure_evidence_report_format(record,failure_evidence_render_line,&layout))return 0;
#ifdef PORTABLE_CRASH_REPORT_SD
 failure_archive_report_status(&layout);
#endif
 return (layout.line+layout.rows-1)/layout.rows;
}
static bool failure_evidence_paint(const risc_failure_evidence_v1 *record,unsigned page,unsigned pages) {
 if(native_custody_retained || failed || surface.frame || paper_token || !display_settled || !pp_enabled())return false;
 paper_previous_valid=previous_valid=alarm_pixels_valid=false;
 #ifdef PORTABLE_RASTER_SNAPSHOT
 if(!raster_immediate())return false;
#endif
 pp_begin();if(native_custody_retained || failed || !surface.frame)return false;
 pp_text(24,36,width()-48,"CRASH / RESTART",1,true,true);fill(24,72,width()-48,2,0);
 (void)failure_evidence_content(record,page,true);
 fill(24,height()-92,width()-48,2,0);
 if(page)pp_text(24,height()-68,width()/2-36,"PREVIOUS",1,true,true);
 pp_text(width()/2,height()-68,width()/2-24,page+1<pages?"NEXT":"CONTINUE",1,true,true);
 char footer[80];snprintf(footer,sizeof(footer),"%u / %u  -  Tap a button or press Confirm",page+1,pages);
 pp_text(24,height()-36,width()-48,footer,1|PAPER_TEXT_LITERAL,false,true);
 if(native_custody_retained || failed)return false;
 resident_failure_present=true;present(true);resident_failure_present=false;
 if(native_custody_retained || failed)return false;
 return portable_paper_frame_drain() && !native_custody_retained && !failed && !surface.frame && display_settled;
}
/* 0: suffix unavailable, use legacy fallback. 1: checked/acknowledged. -1:
 * unsafe or failed; evidence stays pending across fresh host ELF instances. */
static int failure_evidence_show(void) {
 if(!rt || rt->struct_size<RISC_RUNTIME_FAILURE_EVIDENCE_V1_SIZE || !rt->failure_evidence)return 0;
 risc_failure_evidence_client_v1 client={.struct_size=sizeof(client)};
 if(!rt->failure_evidence(&client))return native_custody_retained || failed?-1:0;
 if(client.api_version!=RISC_FAILURE_EVIDENCE_API_V1 || client.struct_size<sizeof(client) ||
    !client.invocation || !client.read || !client.acknowledge)return 0;
 for(;;) {
  if(native_custody_retained || failed)return -1;
  risc_failure_evidence_v1 record={.struct_size=sizeof(record)};
  int status=client.read(client.invocation,&record);
  if(native_custody_retained || failed)return -1;
  if(status==RISC_FAILURE_EVIDENCE_NONE)return 1;
  if(status!=RISC_FAILURE_EVIDENCE_OK || record.api_version!=RISC_FAILURE_EVIDENCE_API_V1 || record.struct_size<sizeof(record))return -1;
  if(!(record.flags&RISC_FAILURE_PENDING))return 1;
  record.application[sizeof(record.application)-1]=0;record.detail[sizeof(record.detail)-1]=0;
#ifdef PORTABLE_CRASH_REPORT_SD
  if(!failure_archive_capture(&record,&client))return -1;
#endif
  unsigned page=0,pages=failure_evidence_content(&record,0,false);
  if(!failure_evidence_paint(&record,page,pages))return -1;
  resident_neutral();bool armed=false,acknowledge=false;
  while(!acknowledge) {
   if(native_custody_retained || failed)return -1;
   bool consumed=false;if(!alarm_foreground(&consumed))return -1;
   if(native_custody_retained || failed)return -1;
   if(consumed){if(!failure_evidence_paint(&record,page,pages))return -1;resident_neutral();armed=false;}
   portable_touch_sample sample;portable_touch_read(&touch,&sample);
   if(native_custody_retained || failed)return -1;
#ifdef PORTABLE_PAPER_PREFERENCES
   paper_orient_input(&sample);
#endif
   bool next=false,previous=false;
   if(sample.valid && sample.released && sample.tap_eligible && !sample.moved && !sample.cancelled &&
      sample.x>=24 && sample.x<width()-24 && sample.y>=height()-92) {
    next=sample.x>=width()/2;previous=!next;
   }
#ifdef PORTABLE_INPUT_NAVIGATION
   if(navigation_ready) {
    risc_input_navigation_frame_v1 input={0};
    if(!navigation->poll(navigation->context,&input)){portable_adapter_retain();return -1;}
    if(native_custody_retained || failed)return -1;
    if(armed) {
     next|=(input.pressed&RISC_NAV_CONFIRM)!=0;
     if(page+1<pages)next|=(input.pressed&(RISC_NAV_RIGHT|RISC_NAV_DOWN|RISC_NAV_PAGE_FORWARD))!=0;
     previous|=(input.pressed&(RISC_NAV_BACK|RISC_NAV_LEFT|RISC_NAV_UP|RISC_NAV_PAGE_BACK))!=0;
    }
    if(!input.buttons && !input.pressed)armed=true;
   }
#else
   (void)armed;
#endif
   if(previous && page) {--page;next=false;}
   else if(next && page+1<pages)++page;
   else if(next) {acknowledge=true;break;}
   else {rt->yield_ms(20);continue;}
   if(!failure_evidence_paint(&record,page,pages))return -1;
   resident_neutral();armed=false;
  }
  /* Every page is physically complete, and the final Continue was explicit.
   * Stale acknowledgement rereads/reviews the new record; it never dismisses it. */
  if(native_custody_retained || failed || surface.frame || paper_token || !display_settled)return -1;
#ifdef PORTABLE_CRASH_REPORT_SD
  status=failure_archive_acknowledge();
#else
  status=client.acknowledge(client.invocation,record.record_boot,record.record_sequence);
#endif
  if(native_custody_retained || failed)return -1;
  if(status==RISC_FAILURE_EVIDENCE_STALE)continue;
  if(status!=RISC_FAILURE_EVIDENCE_OK)return -1;
  if(!resident_focus(true))return -1;
  resident_neutral();paper_previous_valid=previous_valid=alarm_pixels_valid=false;return 1;
 }
}
#else
static int failure_evidence_show(void){return 0;}
#endif

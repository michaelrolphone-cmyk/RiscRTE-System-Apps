/* Host-owned diagnostics use copied Runtime evidence only. This is never a
 * panic handler: retained invocations and an unsafe display cannot paint. */
static bool resident_prior_failure_checked;
static const char *resident_failure_step(uint32_t kind) {
 switch(kind) {
 case RISC_RESIDENT_FAILURE_LOAD:return "LOAD APPLICATION";
 case RISC_RESIDENT_FAILURE_ABI:return "CHECK APPLICATION ABI";
 case RISC_RESIDENT_FAILURE_INIT:return "INITIALIZE APPLICATION";
 case RISC_RESIDENT_FAILURE_CLEANUP:return "CLEAN UP APPLICATION";
 case RISC_RESIDENT_FAILURE_ALLOCATION:return "ALLOCATE APPLICATION";
 default:return NULL;
 }
}
static bool resident_failure_copy(risc_resident_failure_v1 *out,const risc_resident_failure_v1 *source) {
 if(!source || source->struct_size!=sizeof(*source) || source->status==RISC_RESIDENT_RETAINED ||
    (source->kind!=RISC_RESIDENT_FAILURE_PRIOR_RESET && source->status>=0) ||
    (!resident_failure_step(source->kind) &&
     !(source->kind==RISC_RESIDENT_FAILURE_PRIOR_RESET && source->native_reason)))return false;
 *out=*source;
 out->application[sizeof(out->application)-1]=0;
 out->detail[sizeof(out->detail)-1]=0;
 return true;
}
/* Wrap on spaces, with character fallback for long paths/tokens. No allocation
 * or font/file provider is needed. A bounded page shows an explicit ellipsis
 * if pathological wide text exhausts its field's lines. */
static int resident_failure_text(int y,const char *value) {
 char line[96];unsigned used=0,lines=0;
 unsigned line_limit=(unsigned)(height()-400)/44;
 if(line_limit>6)line_limit=6;
 if(!line_limit)line_limit=1;
 for(unsigned i=0;i<193 && value[i];++i) {
  unsigned char c=(unsigned char)value[i];
  if(c<32 || c>126)c=' ';
  if(!used && c==' ')continue;
  line[used]=(char)c;line[used+1]=0;
  if(used && (used==sizeof(line)-2 || pp_measure(line,false)>width()-48)) {
   unsigned split=used;while(split && line[split-1]!=' ')--split;
   unsigned count=split?split-1:used;line[count]=0;
   if(++lines>=line_limit) {
    if(count>=3)memcpy(line+count-3,"...",3);
    pp_text(24,y,width()-48,line,1|PAPER_TEXT_LITERAL,false,true);return y+22;
   }
   pp_text(24,y,width()-48,line,1|PAPER_TEXT_LITERAL,false,true);y+=22;
   unsigned tail=split?used-split:0;
   if(tail)memmove(line,line+split,tail);
   used=tail;line[used]=(char)c;line[used+1]=0;
  }
  if(!used && c==' ')continue;
  ++used;
 }
 if(used){line[used]=0;pp_text(24,y,width()-48,line,1|PAPER_TEXT_LITERAL,false,true);y+=22;}
 return y;
}
static bool resident_failure_paint(const risc_resident_failure_v1 *record) {
 if(native_custody_retained || failed || surface.frame || paper_token || !display_settled || !pp_enabled())return false;
 paper_previous_valid=previous_valid=alarm_pixels_valid=false;
 #ifdef PORTABLE_RASTER_SNAPSHOT
 if(!raster_immediate())return false;
#endif
 pp_begin();if(failed || native_custody_retained || !surface.frame)return false;
 const bool prior=record->kind==RISC_RESIDENT_FAILURE_PRIOR_RESET;
 pp_text(24,36,width()-48,prior?"PREVIOUS RESTART":"APPLICATION FAILURE",1,true,true);
 fill(24,72,width()-48,2,0);
 char line[96];
 if(prior)snprintf(line,sizeof(line),"NATIVE RESET REASON: %lu",(unsigned long)record->native_reason);
 else snprintf(line,sizeof(line),"FAILED STEP: %s",resident_failure_step(record->kind));
 int y=resident_failure_text(96,line)+18;
 pp_text(24,y,width()-48,"APPLICATION",1,true,true);y+=28;
 y=resident_failure_text(y,record->application[0]?record->application:"Not recorded");y+=18;
 pp_text(24,y,width()-48,"DETAIL",1,true,true);y+=28;
 y=resident_failure_text(y,record->detail[0]?record->detail:"Not recorded");y+=18;
 if(!prior) {
  snprintf(line,sizeof(line),"STATUS: %ld",(long)record->status);y=resident_failure_text(y,line);
  if(record->invocation) {
   snprintf(line,sizeof(line),"INVOCATION: %llu",(unsigned long long)record->invocation);
   y=resident_failure_text(y,line);
  }
 }
 (void)resident_failure_text(y+8,"Stack: not recorded");
 fill(24,height()-92,width()-48,2,0);
 pp_text(24,height()-70,width()-48,"CONTINUE",1,true,true);
 pp_text(24,height()-40,width()-48,"Tap here or press Home / Back",1|PAPER_TEXT_LITERAL,false,true);
 if(native_custody_retained || failed)return false;
 resident_failure_present=true;present(true);resident_failure_present=false;
 if(native_custody_retained || failed)return false;
 return portable_paper_frame_drain() && !surface.frame && display_settled;
}
static bool resident_failure_show(const risc_resident_failure_v1 *source) {
 risc_resident_failure_v1 record;
 if(!resident_failure_copy(&record,source))return true;
 if(!resident_failure_paint(&record))return false;
 resident_neutral();bool navigation_armed=false;
 for(;;) {
  if(native_custody_retained || failed)return false;
  bool alarm_consumed=false;
  if(!alarm_foreground(&alarm_consumed))return false;
  if(native_custody_retained || failed)return false;
  if(alarm_consumed) {
   if(!resident_failure_paint(&record))return false;
   resident_neutral();navigation_armed=false;
  }
  portable_touch_sample sample;portable_touch_read(&touch,&sample);
  if(native_custody_retained || failed)return false;
#ifdef PORTABLE_PAPER_PREFERENCES
  paper_orient_input(&sample);
#endif
  bool dismiss=sample.home_pressed || (sample.valid && sample.released &&
   sample.tap_eligible && !sample.moved && !sample.cancelled &&
   sample.x>=24 && sample.x<width()-24 && sample.y>=height()-92);
#ifdef PORTABLE_INPUT_NAVIGATION
  if(navigation_ready) {
   risc_input_navigation_frame_v1 input={0};
   if(!navigation->poll(navigation->context,&input)){portable_adapter_retain();return false;}
   if(native_custody_retained || failed)return false;
   if(navigation_armed && (input.pressed&(RISC_NAV_HOME|RISC_NAV_BACK|RISC_NAV_CONFIRM)))dismiss=true;
   if(!input.buttons && !input.pressed)navigation_armed=true;
  }
#else
  (void)navigation_armed;
#endif
  if(dismiss)break;
  rt->yield_ms(20);
 }
 /* Do not replay the acknowledgement into Home, sleep or Quick Actions. */
 if(!resident_focus(true))return false;
 resident_neutral();paper_previous_valid=previous_valid=alarm_pixels_valid=false;
 return true;
}
#include "failure_evidence.inc"
static bool resident_prior_failure(void) {
 if(resident_prior_failure_checked)return true;
 resident_prior_failure_checked=true;
 if(native_custody_retained || failed)return false;
 int evidence=failure_evidence_show();
 if(evidence)return evidence>0;
 if(!resident.last_failure)return true;
 risc_resident_failure_v1 record={.struct_size=sizeof(record)};
 if(!resident.last_failure(resident.invocation,&record))return !native_custody_retained && !failed;
 if(record.kind!=RISC_RESIDENT_FAILURE_PRIOR_RESET)return true;
 return resident_failure_show(&record);
}

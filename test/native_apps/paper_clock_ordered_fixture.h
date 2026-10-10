#pragma once
/* Test-only raw provider. Scheduled changes are controller reports, with no
 * additional unchanged reports between them. Each report publishes all contacts
 * and every ordered edge before its watermark; capture never discards a tap. */
static risc_touch_snapshot_v1 clock_input_state;
static risc_touch_event_v1 clock_input_queue[RISC_TOUCH_QUEUE_LENGTH];
static unsigned clock_input_head,clock_input_count,clock_input_at,clock_input_delivered[6];
static void clock_input_emit(unsigned kind,risc_touch_contact_v1 point,unsigned at) {
 assert(clock_input_count<RISC_TOUCH_QUEUE_LENGTH);
 clock_input_queue[(clock_input_head+clock_input_count++)%RISC_TOUCH_QUEUE_LENGTH]=
  (risc_touch_event_v1){.sequence=++clock_input_state.sequence,.timestamp_ms=at,
   .kind=kind,.id=point.id,.x=point.x,.y=point.y};
}
static int clock_input_find(const risc_touch_snapshot_v1 *state,unsigned id) {
 for(unsigned i=0;i<state->contact_count;i++)if(state->contacts[i].id==id)return (int)i;
 return -1;
}
static void clock_input_report(risc_touch_snapshot_v1 next,unsigned at) {
 unsigned before=clock_input_state.sequence;
 assert(next.contact_count<=RISC_TOUCH_MAX_CONTACTS);
 for(unsigned i=0;i<next.contact_count;i++) {
  assert(next.contacts[i].x<next.width&&next.contacts[i].y<next.height);
  for(unsigned j=0;j<i;j++)assert(next.contacts[i].id!=next.contacts[j].id);
 }
 for(unsigned i=0;i<clock_input_state.contact_count;i++) {
  risc_touch_contact_v1 old=clock_input_state.contacts[i];
  int found=clock_input_find(&next,old.id);
  if(found<0)clock_input_emit(RISC_TOUCH_EVENT_UP,old,at);
  else if(old.x!=next.contacts[found].x||old.y!=next.contacts[found].y)
   clock_input_emit(RISC_TOUCH_EVENT_MOVE,next.contacts[found],at);
 }
 for(unsigned i=0;i<next.contact_count;i++)if(clock_input_find(&clock_input_state,next.contacts[i].id)<0)
  clock_input_emit(RISC_TOUCH_EVENT_DOWN,next.contacts[i],at);
 for(unsigned bit=0;bit<32;bit++)if((clock_input_state.buttons^next.buttons)&(1u<<bit))
  clock_input_emit(next.buttons&(1u<<bit)?RISC_TOUCH_EVENT_BUTTON_DOWN:RISC_TOUCH_EVENT_BUTTON_UP,
   (risc_touch_contact_v1){.id=(uint8_t)bit},at);
 next.sequence=clock_input_state.sequence;
 next.timestamp_ms=before==next.sequence?clock_input_state.timestamp_ms:at;
 clock_input_state=next;
}
static uint64_t clock_input_subscribe(void *c) {
 uint64_t id=subscribe(c);assert(snapshot(c,&clock_input_state));
 clock_input_head=clock_input_count=0;clock_input_at=script_ms;return id;
}
static bool clock_input_poll(void *c,size_t count) {
 assert(poll_touch(c,count));
 /* Sample every elapsed script millisecond, including reports captured between
  * logical controller passes. A fast tap remains DOWN then UP in the queue. */
 unsigned current=script_ms;
 while(clock_input_at<current) {
  script_ms=++clock_input_at;risc_touch_snapshot_v1 next;assert(snapshot(c,&next));
  clock_input_report(next,ms-current+clock_input_at);
 }
 script_ms=current;return true;
}
static bool clock_input_snapshot(void *c,risc_touch_snapshot_v1 *out) {
 (void)c;*out=clock_input_state;return true;
}
static int32_t clock_input_next(void *c,uint64_t id,risc_touch_event_v1 *out) {
 (void)c;assert(id==1);
#ifdef TEST_CLOCK_DROP_ORDERED_EDGES
 /* Negative control: retain authoritative watermarks but lose provider edges. */
 clock_input_head=clock_input_count=0;
#endif
 if(!clock_input_count)return 0;
 *out=clock_input_queue[clock_input_head];clock_input_head=(clock_input_head+1)%RISC_TOUCH_QUEUE_LENGTH;
 --clock_input_count;assert(out->kind<6);clock_input_delivered[out->kind]++;return 1;
}

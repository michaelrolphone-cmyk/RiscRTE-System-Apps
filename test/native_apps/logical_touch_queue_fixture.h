#pragma once
/* A strict raw provider: collect captures every edge and next delivers exactly
 * one. Snapshot and event sequence/timestamp describe the same event stream. */
static bool (*logical_snapshot_script)(void *,risc_touch_snapshot_v1 *);
static risc_touch_snapshot_v1 logical_raw_state;
static risc_touch_event_v1 logical_raw_queue[2048];
static unsigned logical_raw_head,logical_raw_tail;
static void logical_raw_emit(unsigned kind,unsigned id,unsigned x,unsigned y){
 assert(logical_raw_tail<sizeof(logical_raw_queue)/sizeof(*logical_raw_queue));
 logical_raw_queue[logical_raw_tail++]=(risc_touch_event_v1){.kind=kind,.id=id,.x=x,.y=y,
  .sequence=++logical_raw_state.sequence,.timestamp_ms=ticks};
 logical_raw_state.timestamp_ms=ticks;
}
static bool logical_raw_poll(void *c,size_t limit){
 (void)limit;io();risc_touch_snapshot_v1 next={0};assert(logical_snapshot_script(c,&next));
 assert(next.contact_count<=1&&logical_raw_state.contact_count<=1);
 const risc_touch_contact_v1 *old=&logical_raw_state.contacts[0],*now=&next.contacts[0];
 if(logical_raw_state.contact_count&&(!next.contact_count||old->id!=now->id))logical_raw_emit(RISC_TOUCH_EVENT_UP,old->id,old->x,old->y);
 if(next.contact_count){
  if(!logical_raw_state.contact_count||old->id!=now->id)logical_raw_emit(RISC_TOUCH_EVENT_DOWN,now->id,now->x,now->y);
  else if(old->x!=now->x||old->y!=now->y)logical_raw_emit(RISC_TOUCH_EVENT_MOVE,now->id,now->x,now->y);
 }
 logical_raw_state.width=next.width;logical_raw_state.height=next.height;
 logical_raw_state.contact_count=next.contact_count;logical_raw_state.contacts[0]=*now;return true;
}
static int32_t logical_raw_next(void *c,uint64_t subscription,risc_touch_event_v1 *out){
 (void)c;(void)subscription;io();if(logical_raw_head==logical_raw_tail)return 0;
 *out=logical_raw_queue[logical_raw_head++];return 1;
}
static bool logical_raw_snapshot(void *c,risc_touch_snapshot_v1 *out){
 (void)c;io();*out=logical_raw_state;return true;
}
static void logical_raw_init(unsigned width,unsigned height,bool (*script)(void *,risc_touch_snapshot_v1 *)){
 logical_snapshot_script=script;logical_raw_state=(risc_touch_snapshot_v1){.width=width,.height=height};logical_raw_head=logical_raw_tail=0;
}

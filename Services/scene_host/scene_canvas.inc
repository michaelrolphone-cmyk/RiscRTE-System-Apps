/* One provider, one physical foreground owner; existing canvas algorithms are
 * reused unchanged. Wrappers join their custody to every scene prefix. */
static bool shared_canvas_clean(void){return !shared_canvas.frames.retained&&!shared_canvas.frames.session&&!shared_canvas.frames.lease&&!shared_canvas.frames.receipts&&!shared_canvas.subscription&&!shared_canvas.claimed;}
static bool shared_canvas_alive(void*unused){(void)unused;return alive();}
static bool shared_canvas_cooperate(void*unused){
 (void)unused;if(!alive()||!shared_owner||!shared_owner->yield_ms)return false;
 shared_owner->yield_ms(1);return alive();
}
static int32_t shared_canvas_enter(bool opening){
 if(!alive())return RISC_CANVAS_RETAINED;
 if(!started)return RISC_CANVAS_UNAVAILABLE;
 if(canvas_call||scene_opening||active||pending||rasterizing||subscription||nav_claimed)return RISC_CANVAS_BUSY;
 if(opening&&shared_canvas.frames.session)return RISC_CANVAS_BUSY;
 canvas_call=true;
 return alive()?RISC_CANVAS_OK:RISC_CANVAS_RETAINED;
}
static int32_t shared_canvas_leave(int32_t result){
 if(!alive()||result==RISC_CANVAS_RETAINED){(void)fail_retained();result=RISC_CANVAS_RETAINED;}
 canvas_call=false;
 if(!retained&&shared_canvas_clean())shared_owner=NULL;
 return result;
}
#define CANVAS_WRAP(name,args,call) static int32_t shared_##name args { (void)context;int32_t rc=shared_canvas_enter(false);if(rc)return rc;return shared_canvas_leave(call); }
static int32_t shared_open(void*context,const risc_canvas_open_v1*request,uint64_t*out){
 (void)context;int32_t rc=shared_canvas_enter(true);if(rc)return rc;
 return shared_canvas_leave(canvas_provider_open(&shared_canvas,request,out));
}
CANVAS_WRAP(geometry,(void*context,uint64_t s,risc_canvas_geometry_v1*out),canvas_provider_geometry(&shared_canvas,s,out))
CANVAS_WRAP(begin,(void*context,uint64_t s,risc_canvas_frame_v1*out),canvas_provider_begin(&shared_canvas,s,out))
CANVAS_WRAP(submit,(void*context,uint64_t s,uint64_t lease,uint64_t*out),canvas_provider_submit(&shared_canvas,s,lease,out))
CANVAS_WRAP(cancel,(void*context,uint64_t s,uint64_t lease),canvas_provider_cancel(&shared_canvas,s,lease))
CANVAS_WRAP(poll,(void*context,uint64_t s,uint64_t receipt,risc_canvas_receipt_v1*out),canvas_provider_poll(&shared_canvas,s,receipt,out))
CANVAS_WRAP(forget,(void*context,uint64_t s,uint64_t receipt),canvas_provider_forget(&shared_canvas,s,receipt))
CANVAS_WRAP(next,(void*context,uint64_t s,risc_canvas_event_v1*out),canvas_provider_next(&shared_canvas,s,out))
CANVAS_WRAP(contacts,(void*context,uint64_t s,risc_canvas_event_v1*out,size_t capacity,size_t*count,uint64_t*sequence),canvas_provider_contacts(&shared_canvas,s,out,capacity,count,sequence))
CANVAS_WRAP(quiesce,(void*context,uint64_t s),canvas_provider_quiesce(&shared_canvas,s))
CANVAS_WRAP(resume,(void*context,uint64_t s),canvas_provider_resume(&shared_canvas,s))
CANVAS_WRAP(close,(void*context,uint64_t s),canvas_provider_close(&shared_canvas,s))
#undef CANVAS_WRAP
static const risc_canvas_api_v1 shared_canvas_api={1,sizeof(shared_canvas_api),RISC_CANVAS_TAG,NULL,shared_open,shared_geometry,shared_begin,shared_submit,shared_cancel,shared_poll,shared_forget,shared_next,shared_contacts,shared_quiesce,shared_resume,shared_close};

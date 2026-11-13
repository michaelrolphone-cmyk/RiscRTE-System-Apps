static int32_t pointer_open(void *c,const risc_components_document_v1 *d,const risc_scene_navigation_v1 *p,uint64_t *out){
 if(!alive())return RISC_SCENE_RETAINED;
 if(active||hold_busy||hold_fault)return RISC_SCENE_BUSY;
 pointer_enabled=true;pointer_pending=false;pointer_reset(false);
 pointer_frame_identity=pointer_presented_identity=(pointer_identity){0};pointer_last=0;pointer_gap=pointer_clock_seen=false;
 int32_t r=hold_open(c,d,p,out);
 if(r!=RISC_SCENE_OK&&!active&&!retained)pointer_enabled=false;
 return r;
}
static int32_t pointer_update(void *c,uint64_t s,const risc_components_document_v1 *d){
 if(!pointer_enabled)return RISC_SCENE_INVALID;
 return hold_update(c,s,d);
}
static int32_t pointer_deliver(risc_scene_pointer_event_v1 *out){
 if(!alive())return RISC_SCENE_RETAINED;
 *out=pointer_packet;pointer_pending=false;
 if(hold_fault){out->intent.flags=RISC_HOLD_LOCAL_FAULT_PENDING;hold_fault_delivered=true;}
 return RISC_SCENE_OK;
}
static int32_t pointer_next(void *c,uint64_t s,risc_scene_pointer_event_v1 *out){
 if(!out||out->struct_size!=sizeof(*out))return RISC_SCENE_INVALID;
 if(!pointer_enabled)return RISC_SCENE_INVALID;
 if(hold_busy)return fail_retained();
 if(pointer_pending){hold_dispatch=true;int32_t r=check(s);hold_dispatch=false;if(r)return r;return pointer_deliver(out);}
 risc_scene_hold_event_v1 e={.struct_size=sizeof(e)};
 int32_t r=hold_next(c,s,&e);
 if(retained)return RISC_SCENE_RETAINED;
 if(pointer_pending)return pointer_deliver(out);
 if(r==RISC_SCENE_OK)*out=(risc_scene_pointer_event_v1){.struct_size=sizeof(*out),.kind=RISC_POINTER_INTENT,.intent=e};
 return r;
}

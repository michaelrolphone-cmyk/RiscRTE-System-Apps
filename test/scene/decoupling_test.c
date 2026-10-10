/* Source-bound scheduling, current hit-map and raster equivalence witnesses.
 * Physical transport cadence is qualified separately. */
#define SCENE_EMBED_KEYBOARD_TEST
#include "keyboard_test.c"
#undef SCENE_EMBED_KEYBOARD_TEST
static void drain_visual(void){
    risc_scene_event_v1 e;
    for(unsigned i=0;i<2000;i++){
        assert(tick(&e)==RISC_SCENE_IDLE);ms+=10;
        risc_scene_navigation_v1 n={.struct_size=sizeof(n)};uint32_t flags;
        assert(!api->snapshot(NULL,session,&n,&flags));
        if(!(flags&RISC_SCENE_PRESENTING))return;
    }
    assert(!"visual catch-up timeout");
}
static void ack_only(void){++doc.revision;assert(!api->update(NULL,session,&doc));}
static void profile_setup(unsigned f,unsigned rotation){
    format=f;w=(rotation==90||rotation==270)?800:480;h=(rotation==90||rotation==270)?480:800;
    prof=(risc_scene_profile_v1){1,sizeof(prof),f,3,88,20,0,65535,0,rotation,0,0};
}
static void push_edge(unsigned kind,unsigned id,unsigned row,unsigned col){
    if(event_at==event_count)event_at=event_count=0;
    assert(event_count<40);unsigned x,y;key_center(row,col,&x,&y);
    events[event_count++]=(risc_touch_event_v1){++seq,ms,kind,id,x,y};
}
static void ordered_overlap(void){
    risc_scene_event_v1 e;
    /* Short contacts and reverse release order remain DOWN-ordered. */
    push_edge(1,3,0,0);push_edge(1,4,0,1);push_edge(3,4,0,1);
    assert(tick(&e)==RISC_SCENE_IDLE);
    push_edge(3,3,0,0);expect_key('Q');ack_only();expect_key('W');ack_only();
    /* Layer acknowledgement changes logical hit mapping while the old frame
     * is still being rasterized or physically refreshed. */
    push_edge(1,0,2,0);push_edge(3,0,2,0);push_edge(1,1,0,0);push_edge(3,1,0,0);
    expect_key(RISC_SCENE_KEY_LAYER);doc.nodes[0].value=1;ack_only();expect_key('1');ack_only();
    /* A genuine subscription gap cancels an in-flight contact. */
    push_edge(1,0,0,1);assert(tick(&e)==RISC_SCENE_IDLE);
    queue_gap=true;push_edge(3,0,0,1);assert(tick(&e)==RISC_SCENE_IDLE);
    assert(tick(&e)==RISC_SCENE_IDLE);push_edge(1,0,0,1);push_edge(3,0,0,1);expect_key('2');ack_only();
    nav_pressed=RISC_NAV_HOME;assert(tick(&e)==RISC_SCENE_OK&&e.kind==RISC_SCENE_SUSPEND_EVENT);
}
static uint64_t digest(uint64_t h,uint64_t v){for(unsigned i=0;i<8;i++){h^=(uint8_t)v;h*=1099511628211ull;v>>=8;}return h;}
static void cadence(unsigned period,unsigned count,unsigned duration){
    risc_scene_event_v1 e;uint64_t result=1469598103934665603ull;unsigned received=0;
    poll_step=0;presentation_delay=duration;ms=0;
    for(unsigned t=0;t<period*count+40;t++){
        ms=t;
        if(t/period<count&&t%period==0){push_edge(1,0,0,(t/period)%2);held_input=true;}
        if(t/period<count&&t%period==20){push_edge(3,0,0,(t/period)%2);held_input=false;}
        int r=tick(&e);assert(r==RISC_SCENE_IDLE||r==RISC_SCENE_OK);
        if(r==RISC_SCENE_OK){
            assert(received<count&&e.kind==RISC_SCENE_VALUE_EVENT&&e.value==(received%2?'W':'Q'));
            assert(t==received*period+20);result=digest(digest(result,t),(unsigned)e.value);++received;
            snprintf(doc.nodes[0].text,sizeof(doc.nodes[0].text),"%u",received);ack_only();
        }
    }
    assert(received==count);presentation_delay=0;allow_complete=true;drain_visual();
    printf("cadence period=%u count=%u panel=%u digest=%016llx frames=%u\n",period,count,duration,(unsigned long long)result,submits);
}
int main(int argc,char **argv){
    assert(argc>=4);profile_setup((unsigned)atoi(argv[2]),(unsigned)atoi(argv[3]));
    startup();keyboard_document();
    if(!strcmp(argv[1],"raster")){
        strcpy(doc.nodes[0].text,"Nova7 Aa 42 !?");doc.nodes[0].value=argc>5?atoi(argv[5]):0;ack_only();drain_visual();
        assert(argc>=5);FILE *f=fopen(argv[4],"wb");assert(f);unsigned bits=format==5?16:format==4?8:format==3?4:format==2?2:1;
        assert(fwrite(pixels+16,1,(w*bits+7)/8*h,f)==(w*bits+7)/8*h);fclose(f);
    }else if(!strcmp(argv[1],"overlap")){allow_complete=false;ordered_overlap();allow_complete=true;}
    else if(!strcmp(argv[1],"cadence")){assert(argc==7);cadence(atoi(argv[4]),atoi(argv[5]),atoi(argv[6]));}
    else assert(0);
    int r=api->close(NULL,session);while(r==RISC_SCENE_AGAIN){allow_complete=true;presentation_delay=0;ms+=10000;r=api->close(NULL,session);}assert(r==RISC_SCENE_OK);assert(driver->quiesce());driver->stop();
    return 0;
}

/* Shared semantic presenter. No application/domain code or product IDs. */
#include "RiscSceneV1.h"
#include "RiscProviderV2.h"
#include "RiscRuntimeV1.h"
#include "RiscDisplayOutputV1.h"
#include "RiscTouchV1.h"
#include "RiscInputNavigationV1.h"
#include "RiscPlatformClockV1.h"
#include "SceneProfileV1.h"
#include "SceneKeyboardV1.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "glyphs.inc"

#define MAX_HITS (RISC_SCENE_MAX_NODES*4u+4u)
enum { HIT_BACK=1,HIT_HOME,HIT_PREVIOUS,HIT_NEXT,HIT_ACTION,HIT_VALUE,HIT_LINK };
typedef struct { int x,y,w,h; uint32_t kind,node,target; int32_t value; } hit;
typedef struct { hit items[MAX_HITS]; unsigned count; uint32_t revision,route,epoch; bool keyboard; } hit_map;
static const risc_display_output_api_v1 *display;
static const risc_touch_api_v1 *touch;
static const risc_input_navigation_api_v1 *navigation;
static const risc_platform_clock_api_v1 *clock_api;
static const risc_scene_profile_v1 *profile;
static risc_display_info_v1 info;
static risc_display_surface_v1 surface;
static risc_display_present_token_v1 present_token;
static risc_scene_document_v1 document;
static risc_scene_navigation_v1 path;
static hit_map visible,upload;
static risc_scene_event_v1 queued;
static uint64_t session,serial,event_serial,subscription,last_sequence;
static uint32_t epoch;
static unsigned width,height,page,focus_part;
static bool started,active,dirty,pending,closing,retained,nav_claimed,have_event;
static bool neutral,contact,moved,keyboard_waiting;
static uint8_t contact_id;
static int down_x,down_y;
static uint32_t down_revision,down_epoch;
static uint64_t down_at;
#include "keyboard.inc"

static bool terminated(const char *s,size_t n){return memchr(s,0,n)!=NULL;}
static int route_index(const risc_scene_document_v1 *d,uint32_t id){
    for(unsigned i=0;i<d->route_count;i++)if(d->routes[i].id==id)return (int)i;
    return -1;
}
static int node_index(const risc_scene_document_v1 *d,uint32_t id){
    for(unsigned i=0;i<d->node_count;i++)if(d->nodes[i].id==id)return (int)i;
    return -1;
}
static bool valid_document(const risc_scene_document_v1 *d){
    if(!d||d->api_version!=1||d->struct_size!=sizeof(*d)||!d->revision||!d->root||
       !d->route_count||d->route_count>RISC_SCENE_MAX_ROUTES||d->node_count>RISC_SCENE_MAX_NODES||
       d->reserved[0]||d->reserved[1])return false;
    for(unsigned i=0;i<d->route_count;i++){
        const risc_scene_route_v1 *r=&d->routes[i];
        if(!r->id||!terminated(r->title,sizeof(r->title)))return false;
        for(unsigned j=0;j<i;j++)if(d->routes[j].id==r->id)return false;
    }
    if(route_index(d,d->root)<0)return false;
    for(unsigned i=0;i<d->node_count;i++){
        const risc_scene_node_v1 *n=&d->nodes[i];
        if(!n->id||route_index(d,n->route)<0||n->kind<RISC_SCENE_TEXT_NODE||n->kind>RISC_SCENE_KEYBOARD_NODE||
           n->flags&~15u||!terminated(n->label,sizeof(n->label))||!terminated(n->text,sizeof(n->text)))return false;
        for(unsigned j=0;j<i;j++)if(d->nodes[j].id==n->id)return false;
        if(n->kind==RISC_SCENE_KEYBOARD_NODE){
            if(!n->action||n->target||n->minimum!=0||n->maximum!=3||n->step!=1||n->value<0||n->value>3)return false;
            for(const unsigned char *p=(const unsigned char*)n->text;*p;p++)if(*p<32||*p>126)return false;
            for(unsigned j=0;j<i;j++)if(d->nodes[j].route==n->route&&d->nodes[j].kind==RISC_SCENE_KEYBOARD_NODE)return false;
        }else if(n->kind>=RISC_SCENE_TIME_OF_DAY&&n->kind<=RISC_SCENE_BOOLEAN){
            if(!n->action||n->target||n->minimum>n->maximum||n->value<n->minimum||
               n->value>n->maximum||n->step<=0)return false;
            if(n->kind==RISC_SCENE_TIME_OF_DAY&&(n->minimum!=0||n->maximum!=1439||n->step!=1))return false;
            if(n->kind==RISC_SCENE_BOOLEAN&&(n->minimum!=0||n->maximum!=1||n->step!=1))return false;
        }else if(n->kind==RISC_SCENE_ACTION){if(!n->action||n->target)return false;}
        else if(n->kind==RISC_SCENE_LINK){if(n->action||route_index(d,n->target)<0)return false;}
        else if(n->action||n->target)return false;
    }
    return true;
}
static bool valid_path(const risc_scene_document_v1 *d,const risc_scene_navigation_v1 *p){
    if(!p||p->api_version!=1||p->struct_size!=sizeof(*p)||p->reserved||!p->depth||
       p->depth>RISC_SCENE_MAX_DEPTH||p->routes[0]!=d->root)return false;
    for(unsigned i=0;i<RISC_SCENE_MAX_DEPTH;i++){
        if(i>=p->depth){if(p->routes[i]||p->focus[i])return false;continue;}
        if(route_index(d,p->routes[i])<0)return false;
        if(p->focus[i]){int k=node_index(d,p->focus[i]);if(k<0||d->nodes[k].route!=p->routes[i])return false;}
    }
    return true;
}
static int32_t fail_retained(void){retained=true;have_event=false;return RISC_SCENE_RETAINED;}
/* A dependency can revoke the native invocation while reporting success.
 * Recheck at every external callback boundary before any further provider I/O. */
static bool alive(void){
    if(retained)return false;
    if(!risc_runtime_get_api(1)){(void)fail_retained();return false;}
    return true;
}
static int32_t check(uint64_t s){
    if(!alive())return RISC_SCENE_RETAINED;
    return started&&active&&s&&s==session?RISC_SCENE_OK:RISC_SCENE_STALE;
}
static uint32_t current_route(void){return path.routes[path.depth-1];}
static bool view_current(void){return visible.revision==document.revision&&visible.route==current_route()&&visible.epoch==epoch;}
/* Word wrapping belongs to presentation, never to application labels. */
static unsigned line_length(const char *s,unsigned columns){
    unsigned n=0,space=0;if(!columns)return 0;
    while(n<columns&&s[n]){if(s[n]==' ')space=n;++n;}
    return s[n]&&space?space:n;
}
static unsigned wrapped_lines(const char *s,unsigned columns){
    unsigned lines=0;if(!columns)return 1;
    while(*s){unsigned n=line_length(s,columns);if(!n)break;++lines;s+=n;while(*s==' ')++s;}
    return lines?lines:1;
}
static unsigned top_height(void);
static unsigned bottom_height(void);
static unsigned row_height(const risc_scene_node_v1 *n){
    if(n->kind==RISC_SCENE_KEYBOARD_NODE)return height-top_height()-bottom_height();
    unsigned scale=profile->font_scale,w=width-2u*profile->padding,needed=0;
    unsigned columns=(w>12?w-12:1)/(6u*scale),line=8u*scale;
    if(n->kind==RISC_SCENE_ACTION||n->kind==RISC_SCENE_LINK)
        needed=wrapped_lines(n->label,columns)*line+12u;
    else if(n->kind==RISC_SCENE_TEXT_NODE)
        needed=(wrapped_lines(n->label,w/(6u*scale))+wrapped_lines(n->text,w/(6u*scale)))*line+8u;
    else if(n->kind==RISC_SCENE_TIME_OF_DAY||n->kind==RISC_SCENE_INTEGER)
        needed=2u*profile->row_height;
    return needed>profile->row_height?needed:profile->row_height;
}
static unsigned top_height(void){return profile->font_scale*7u+profile->padding*2u+8u;}
static unsigned bottom_height(void){return profile->font_scale*7u+profile->padding*2u+8u;}
static unsigned build_pages(unsigned *list,unsigned *starts,unsigned *count){
    *count=0;for(unsigned i=0;i<document.node_count;i++)if(document.nodes[i].route==current_route()&&!(document.nodes[i].flags&RISC_SCENE_HIDDEN))list[(*count)++]=i;
    unsigned pages=1,used=0,available=height-top_height()-bottom_height();starts[0]=0;
    for(unsigned i=0;i<*count;i++){
        unsigned h=row_height(&document.nodes[list[i]]);
        if(used&&used+h>available){starts[pages++]=i;used=0;}
        used+=h;
    }
    starts[pages]=*count;return pages;
}
static const risc_scene_node_v1 *current_keyboard(void){
    unsigned list[RISC_SCENE_MAX_NODES],starts[RISC_SCENE_MAX_NODES+1],count;
    unsigned pages=build_pages(list,starts,&count);
    if(page>=pages)return NULL;
    for(unsigned i=starts[page];i<starts[page+1];i++)
        if(document.nodes[list[i]].kind==RISC_SCENE_KEYBOARD_NODE)return &document.nodes[list[i]];
    return NULL;
}
static bool keyboard_ready(void){return !pending&&!dirty&&!keyboard_waiting&&view_current();}
static void restore_page(void){
    unsigned list[RISC_SCENE_MAX_NODES],starts[RISC_SCENE_MAX_NODES+1],count;
    unsigned pages=build_pages(list,starts,&count);page=0;
    for(unsigned p=0;p<pages;p++)for(unsigned i=starts[p];i<starts[p+1];i++)
        if(document.nodes[list[i]].id==path.focus[path.depth-1])page=p;
}
static void invalidate_view(void){
    dirty=true;contact=false;neutral=false;have_event=false;
    if(epoch==UINT32_MAX){retained=true;return;}++epoch;
}
static int32_t navigate_impl(uint32_t op,uint32_t route){
    if(op==RISC_SCENE_PUSH){
        if(!route||route_index(&document,route)<0)return RISC_SCENE_INVALID;
        if(path.depth==RISC_SCENE_MAX_DEPTH)return RISC_SCENE_BUSY;
        path.routes[path.depth]=route;path.focus[path.depth]=0;++path.depth;
    }else if(op==RISC_SCENE_POP){
        if(route||path.depth==1)return RISC_SCENE_INVALID;
        --path.depth;path.routes[path.depth]=path.focus[path.depth]=0;
    }else if(op==RISC_SCENE_ROOT){
        if(route&&route!=document.root)return RISC_SCENE_INVALID;
        for(unsigned i=1;i<RISC_SCENE_MAX_DEPTH;i++)path.routes[i]=path.focus[i]=0;
        path.depth=1;
    }else return RISC_SCENE_INVALID;
    focus_part=0;keyboard_waiting=false;invalidate_view();restore_page();return retained?RISC_SCENE_RETAINED:RISC_SCENE_OK;
}
static void emit(uint32_t kind,uint32_t node,uint32_t action,int32_t value){
    if(have_event||closing||retained)return;
    if(event_serial==UINT64_MAX){retained=true;return;}
    queued=(risc_scene_event_v1){sizeof(queued),kind,document.revision,node,action,value,++event_serial};have_event=true;
}
static void keyboard_emit(const risc_scene_node_v1 *n,unsigned key){
    if(!keyboard_ready()||have_event)return;
    path.focus[path.depth-1]=n->id;
    emit(RISC_SCENE_VALUE_EVENT,n->id,n->action,(int32_t)key);
    contact=false;neutral=false;
}
static void back(void){
    const risc_scene_node_v1 *keyboard=current_keyboard();
    if(keyboard){keyboard_emit(keyboard,RISC_SCENE_KEY_CANCEL);return;}
    int r=route_index(&document,current_route());
    if(r>=0&&document.routes[r].back_action)emit(RISC_SCENE_ACTION_EVENT,0,document.routes[r].back_action,0);
    else if(path.depth>1)(void)navigate_impl(RISC_SCENE_POP,0);
    else emit(RISC_SCENE_SUSPEND_EVENT,0,0,0);
}
static int32_t changed_value(const risc_scene_node_v1 *n,int direction,int step){
    if(n->kind==RISC_SCENE_TIME_OF_DAY){
        // TIME_OF_DAY is validated to 0..1439; its only input steps are 1/60.
        int32_t v=n->value+direction*step;v%=1440;if(v<0)v+=1440;return v;
    }
    int64_t v=(int64_t)n->value+(int64_t)direction*step;
    if(v<n->minimum)v=n->minimum;
    if(v>n->maximum)v=n->maximum;
    return (int32_t)v;
}
static void turn_page(int direction){
    unsigned list[RISC_SCENE_MAX_NODES],starts[RISC_SCENE_MAX_NODES+1],count;
    unsigned pages=build_pages(list,starts,&count);
    if((direction<0&&!page)||(direction>0&&page+1>=pages))return;
    page=(unsigned)((int)page+direction);
    if(starts[page]<count)path.focus[path.depth-1]=document.nodes[list[starts[page]]].id;
    invalidate_view();
}
static void activate(const hit *h){
    if(!view_current()||have_event||(current_keyboard()&&!keyboard_ready()))return;
    if(h->kind==HIT_BACK){back();return;}
    if(h->kind==HIT_HOME){emit(RISC_SCENE_SUSPEND_EVENT,0,0,0);return;}
    if(h->kind==HIT_PREVIOUS){turn_page(-1);return;}
    if(h->kind==HIT_NEXT){turn_page(1);return;}
    int i=node_index(&document,h->node);if(i<0)return;
    const risc_scene_node_v1 *n=&document.nodes[i];
    if(n->route!=current_route()||n->flags&(RISC_SCENE_DISABLED|RISC_SCENE_HIDDEN))return;
    path.focus[path.depth-1]=n->id;
    if(n->kind==RISC_SCENE_KEYBOARD_NODE){keyboard_emit(n,(unsigned)h->value);return;}
    if(h->kind==HIT_LINK)(void)navigate_impl(RISC_SCENE_PUSH,n->target);
    else if(h->kind==HIT_VALUE)emit(RISC_SCENE_VALUE_EVENT,n->id,n->action,h->value);
    else emit(RISC_SCENE_ACTION_EVENT,n->id,n->action,0);
}
static void tap(int x,int y){
    if(!view_current())return;
    for(unsigned i=0;i<visible.count;i++){
        const hit *h=&visible.items[i];
        if(x>=h->x&&y>=h->y&&x<h->x+h->w&&y<h->y+h->h){activate(h);return;}
    }
}
static bool valid_snapshot(const risc_touch_snapshot_v1 *s){
    if(!s->width||!s->height||s->width>8192||s->height>8192||s->contact_count>RISC_TOUCH_MAX_CONTACTS)return false;
    for(unsigned i=0;i<s->contact_count;i++){
        if(s->contacts[i].x>=s->width||s->contacts[i].y>=s->height)return false;
        for(unsigned j=0;j<i;j++)if(s->contacts[i].id==s->contacts[j].id)return false;
    }
    return true;
}
static risc_touch_snapshot_v1 touch_state;
static bool synchronize_touch(void){
    risc_touch_snapshot_v1 s={0};bool ok=touch->snapshot(touch->context,&s);
    if(!alive()||!ok||!valid_snapshot(&s))return false;
    touch_state=s;last_sequence=s.sequence;contact=false;neutral=!s.contact_count&&!s.buttons;return true;
}
static bool touch_point(uint16_t raw_x,uint16_t raw_y,int *x,int *y){
    uint32_t w=touch_state.width,h=touch_state.height,rx=raw_x,ry=raw_y;
    if(rx>=w||ry>=h)return false;
    uint32_t tx=rx,ty=ry,tw=w,th=h;
    switch(profile->touch_rotation){
    case 90:tx=h-1-ry;ty=rx;tw=h;th=w;break;
    case 180:tx=w-1-rx;ty=h-1-ry;break;
    case 270:tx=ry;ty=w-1-rx;tw=h;th=w;break;
    default:break;
    }
    // Snapshot dimensions <=8192 and display dimensions <=2048: products fit
    // 24 bits. Avoid expensive 64-bit compiler division helpers in this path.
    *x=(int)(tx*width/tw);*y=(int)(ty*height/th);return true;
}
static void process_touch_event(const risc_touch_event_v1 *e){
    if(e->sequence<=last_sequence)return;
    last_sequence=e->sequence;
    int x=0,y=0;
    if(e->kind>=RISC_TOUCH_EVENT_DOWN&&e->kind<=RISC_TOUCH_EVENT_UP&&!touch_point(e->x,e->y,&x,&y)){
        contact=false;neutral=false;return;
    }
    if(e->kind==RISC_TOUCH_EVENT_DOWN){
        if(contact){contact=false;neutral=false;return;}
        if(!neutral||!view_current()||(current_keyboard()&&!keyboard_ready()))return;
        contact=true;contact_id=e->id;down_x=x;down_y=y;down_at=e->timestamp_ms;
        moved=false;down_revision=document.revision;down_epoch=epoch;
    }else if(e->kind==RISC_TOUCH_EVENT_MOVE&&contact&&e->id==contact_id){
        int slop=(int)(width/24u);if(slop<8)slop=8;
        if(x-down_x>slop||down_x-x>slop||y-down_y>slop||down_y-y>slop)moved=true;
        if(current_keyboard())for(unsigned i=0;i<visible.count;i++){
            const hit *h=&visible.items[i];
            if(down_x>=h->x&&down_y>=h->y&&down_x<h->x+h->w&&down_y<h->y+h->h&&
               !(x>=h->x&&y>=h->y&&x<h->x+h->w&&y<h->y+h->h))moved=true;
        }
    }else if(e->kind==RISC_TOUCH_EVENT_UP&&contact&&e->id==contact_id){
        bool accepted=!moved&&down_revision==document.revision&&down_epoch==epoch&&
            e->timestamp_ms>=down_at&&e->timestamp_ms-down_at<=1500;
        contact=false;
        if(accepted&&current_keyboard()){
            const hit *first=NULL,*last=NULL;
            for(unsigned i=0;i<visible.count;i++){
                const hit *h=&visible.items[i];
                if(down_x>=h->x&&down_y>=h->y&&down_x<h->x+h->w&&down_y<h->y+h->h)first=h;
                if(x>=h->x&&y>=h->y&&x<h->x+h->w&&y<h->y+h->h)last=h;
            }
            accepted=first&&first==last;
        }
        if(accepted)tap(x,y);
    }else if(e->kind==RISC_TOUCH_EVENT_BUTTON_DOWN){contact=false;neutral=false;}
}
static int32_t poll_touch(void){
    bool resync=!touch->poll(touch->context,2);unsigned drained=0;
    if(!alive())return RISC_SCENE_RETAINED;
    if(resync){contact=false;neutral=false;}
    for(;drained<16;drained++){
        risc_touch_event_v1 e={0};int32_t r=touch->next(touch->context,subscription,&e);
        if(!alive())return RISC_SCENE_RETAINED;
        if(r==0)break;
        if(r==-2)return fail_retained();
        if(r!=1){resync=true;contact=false;neutral=false;have_event=false;continue;}
        if(!resync)process_touch_event(&e);
        if(retained)return RISC_SCENE_RETAINED;
    }
    if(drained==16)resync=true; /* Never join a gesture across a truncated drain. */
    risc_touch_snapshot_v1 s={0};
    bool ok=touch->snapshot(touch->context,&s);
    if(!alive()||!ok||!valid_snapshot(&s))return fail_retained();
    if(s.width!=touch_state.width||s.height!=touch_state.height||s.contact_count>1||s.buttons)resync=true;
    touch_state=s;
    if(resync){contact=false;neutral=false;have_event=false;last_sequence=s.sequence;}
    if(!contact&&!s.contact_count&&!s.buttons)neutral=true;
    return RISC_SCENE_OK;
}
static void move_focus(int direction){
    unsigned list[RISC_SCENE_MAX_NODES],count=0;int pos=-1;
    for(unsigned i=0;i<document.node_count;i++){
        const risc_scene_node_v1 *n=&document.nodes[i];
        if(n->route!=current_route()||n->kind==RISC_SCENE_TEXT_NODE||n->flags&(RISC_SCENE_DISABLED|RISC_SCENE_HIDDEN))continue;
        if(n->id==path.focus[path.depth-1])pos=(int)count;
        list[count++]=i;
    }
    if(!count)return;
    if(pos<0)pos=direction>0?0:(int)count-1;
    else {pos+=direction;if(pos<0)pos=(int)count-1;if(pos>=(int)count)pos=0;}
    path.focus[path.depth-1]=document.nodes[list[pos]].id;focus_part=0;invalidate_view();restore_page();
}
static void keyboard_move(const risc_scene_node_v1 *n,uint32_t pressed){
    keyboard_key from,key;unsigned count=0;
    while(keyboard_bounds((unsigned)n->value,count,&key))++count;
    if(!count)return;
    if(focus_part>=count)focus_part=0;
    unsigned next=focus_part;
    if(pressed&RISC_NAV_LEFT)next=(focus_part+count-1)%count;
    else if(pressed&RISC_NAV_RIGHT)next=(focus_part+1)%count;
    else {
        (void)keyboard_bounds((unsigned)n->value,focus_part,&from);
        unsigned row=(from.row+(pressed&RISC_NAV_UP?3u:1u))%4u;int distance=INT_MAX;
        for(unsigned i=0;keyboard_bounds((unsigned)n->value,i,&key);i++)if(key.row==row){
            int delta=key.center-from.center;if(delta<0)delta=-delta;
            if(delta<distance){distance=delta;next=i;}
        }
    }
    path.focus[path.depth-1]=n->id;focus_part=next;invalidate_view();
}
static int32_t poll_navigation(void){
    if(!navigation)return RISC_SCENE_OK;
    risc_input_navigation_frame_v1 f={0};bool ok=navigation->poll(navigation->context,&f);
    if(!alive()||!ok)return fail_retained();
    if(f.pressed&RISC_NAV_HOME){emit(RISC_SCENE_SUSPEND_EVENT,0,0,0);return RISC_SCENE_OK;}
    if(!view_current()||have_event)return RISC_SCENE_OK;
    const risc_scene_node_v1 *keyboard=current_keyboard();
    if(keyboard){
        if(!keyboard_ready())return RISC_SCENE_OK;
        if(f.pressed&RISC_NAV_BACK)keyboard_emit(keyboard,RISC_SCENE_KEY_CANCEL);
        else if(keyboard->flags&RISC_SCENE_DISABLED){
            if(f.pressed&RISC_NAV_CONFIRM)keyboard_emit(keyboard,RISC_SCENE_KEY_DONE);
        }else if(f.pressed&(RISC_NAV_LEFT|RISC_NAV_RIGHT|RISC_NAV_UP|RISC_NAV_DOWN))keyboard_move(keyboard,f.pressed);
        else if(f.pressed&(RISC_NAV_PAGE_BACK|RISC_NAV_PAGE_FORWARD))keyboard_emit(keyboard,RISC_SCENE_KEY_LAYER);
        else if(f.pressed&RISC_NAV_CONFIRM){
            keyboard_key key;if(keyboard_bounds((unsigned)keyboard->value,focus_part,&key))keyboard_emit(keyboard,key.key);
        }
        return RISC_SCENE_OK;
    }
    if(f.pressed&RISC_NAV_BACK)back();
    else if(f.pressed&RISC_NAV_UP)move_focus(-1);
    else if(f.pressed&RISC_NAV_DOWN)move_focus(1);
    else if(f.pressed&RISC_NAV_PAGE_BACK)turn_page(-1);
    else if(f.pressed&RISC_NAV_PAGE_FORWARD)turn_page(1);
    else {
        int k=node_index(&document,path.focus[path.depth-1]);
        if(k<0){if(f.pressed)move_focus(1);return RISC_SCENE_OK;}
        const risc_scene_node_v1 *n=&document.nodes[k];
        if(n->flags&(RISC_SCENE_DISABLED|RISC_SCENE_HIDDEN))return RISC_SCENE_OK;
        if(n->kind>=RISC_SCENE_TIME_OF_DAY&&n->kind<=RISC_SCENE_BOOLEAN){
            if(f.pressed&(RISC_NAV_LEFT|RISC_NAV_RIGHT)){
                int direction=f.pressed&RISC_NAV_LEFT?-1:1;
                int step=n->kind==RISC_SCENE_TIME_OF_DAY?(focus_part?1:60):n->step;
                emit(RISC_SCENE_VALUE_EVENT,n->id,n->action,changed_value(n,direction,step));
            }else if(f.pressed&RISC_NAV_CONFIRM){
                if(n->kind==RISC_SCENE_TIME_OF_DAY){focus_part^=1;dirty=true;}
                else emit(RISC_SCENE_VALUE_EVENT,n->id,n->action,n->kind==RISC_SCENE_BOOLEAN?!n->value:changed_value(n,1,n->step));
            }
        }else if(f.pressed&RISC_NAV_CONFIRM){hit h={0};h.node=n->id;h.kind=n->kind==RISC_SCENE_LINK?HIT_LINK:HIT_ACTION;activate(&h);}
    }
    return RISC_SCENE_OK;
}

/* Bounded rasterization directly into a provider-owned surface. */
static uint8_t luminance(uint16_t c){return (uint8_t)((((c>>11)&31)*299u*255u/31u+((c>>5)&63)*587u*255u/63u+(c&31)*114u*255u/31u)/1000u);}
static void pixel(int x,int y,uint16_t color){
    if(x<0||y<0||(unsigned)x>=width||(unsigned)y>=height)return;
    unsigned px=(unsigned)x,py=(unsigned)y;
    switch(profile->display_rotation){
    case 90:px=surface.width-1-(unsigned)y;py=(unsigned)x;break;
    case 180:px=surface.width-1-(unsigned)x;py=surface.height-1-(unsigned)y;break;
    case 270:px=(unsigned)y;py=surface.height-1-(unsigned)x;break;
    default:break;
    }
    uint8_t *row=(uint8_t*)surface.pixels+(size_t)py*surface.stride_bytes;
    if(surface.pixel_format==RISC_DISPLAY_FORMAT_RGB565){row[px*2]=(uint8_t)color;row[px*2+1]=(uint8_t)(color>>8);return;}
    unsigned bits=surface.pixel_format==RISC_DISPLAY_FORMAT_MONO1?1u:surface.pixel_format==RISC_DISPLAY_FORMAT_GRAY2?2u:surface.pixel_format==RISC_DISPLAY_FORMAT_GRAY4?4u:8u;
    unsigned maximum=(1u<<bits)-1u,level=(255u-luminance(color))*maximum/255u;
    unsigned offset=px*bits,shift=8u-bits-(offset&7u),mask=maximum<<shift;
    row[offset>>3]=(uint8_t)((row[offset>>3]&~mask)|(level<<shift));
}
static void fill(int x,int y,int w,int h,uint16_t c){
    int x1=x<0?0:x,y1=y<0?0:y,x2=x+w,y2=y+h;
    if(x2>(int)width)x2=(int)width;
    if(y2>(int)height)y2=(int)height;
    for(int yy=y1;yy<y2;yy++)for(int xx=x1;xx<x2;xx++)pixel(xx,yy,c);
}
static void text(int x,int y,int w,const char *s,unsigned scale,uint16_t color){
    if(!scale)return;
    int maximum=w/(int)(6u*scale);if(maximum>72)maximum=72;
    for(int k=0;k<maximum&&s[k];k++){
        unsigned char c=(unsigned char)s[k];
        for(unsigned col=0;col<5;col++){
            unsigned bits=c>=32&&c<=126?glyphs[c-32][col]:glyphs['?'-32][col];
            for(unsigned row=0;row<7;row++)if(bits&(1u<<row))fill(x+k*(int)(6*scale)+(int)(col*scale),y+(int)(row*scale),(int)scale,(int)scale,color);
        }
    }
}
static void wrapped_text(int x,int y,int w,const char *s,unsigned scale,uint16_t color){
    unsigned columns=w>0?(unsigned)w/(6u*scale):0;if(!columns)return;
    while(*s){
        unsigned n=line_length(s,columns);if(!n)break;
        char line[RISC_SCENE_TEXT+1];if(n>=sizeof(line))n=sizeof(line)-1;
        memcpy(line,s,n);line[n]=0;text(x,y,w,line,scale,color);
        y+=(int)(8u*scale);s+=n;while(*s==' ')++s;
    }
}
static void add_hit(int x,int y,int w,int h,uint32_t kind,uint32_t node,uint32_t target,int32_t value){
    if(upload.count<MAX_HITS)upload.items[upload.count++]=(hit){x,y,w,h,kind,node,target,value};
}
static void button(int x,int y,int w,int h,const char *label,uint32_t kind,const risc_scene_node_v1 *n,int32_t value){
    uint16_t fg=(uint16_t)profile->foreground_rgb565,bg=(uint16_t)profile->background_rgb565,accent=(uint16_t)profile->accent_rgb565;
    bool disabled=n&&(n->flags&(RISC_SCENE_DISABLED|RISC_SCENE_HIDDEN));unsigned scale=profile->font_scale;
    fill(x,y,w,h,accent);fill(x+1,y+1,w-2,h-2,bg);
    if(n&&n->id==path.focus[path.depth-1])fill(x+2,y+2,3,h-4,accent);
    unsigned lines=wrapped_lines(label,w>12?(unsigned)(w-12)/(6u*scale):0);
    int text_height=(int)((lines-1)*8u*scale+7u*scale);
    wrapped_text(x+6,y+(h-text_height)/2,w-12,label,scale,disabled?0x7bef:fg);
    if(!disabled)add_hit(x,y,w,h,kind,n?n->id:0,n?n->target:0,value);
}
static void draw_keyboard(const risc_scene_node_v1 *n,int y,int h){
    int pad=(int)profile->padding,w=(int)width-2*pad;
    unsigned scale=profile->font_scale;uint16_t fg=(uint16_t)profile->foreground_rgb565;
    uint16_t bg=(uint16_t)profile->background_rgb565,accent=(uint16_t)profile->accent_rgb565;
    int label_y=y+2,text_y=label_y+(int)(8u*scale)+2;
    text(pad,label_y,w,n->label,scale,fg);
    /* Keep the insertion end visible in the small viewport, without changing
     * the provider-owned draft or introducing input-format variants. */
    unsigned columns=(unsigned)w/(6u*scale);size_t length=strlen(n->text);
    const char *tail=n->text+(length>columns?length-columns:0);
    text(pad,text_y,w,tail,scale,fg);
    if(n->flags&RISC_SCENE_DISABLED){
        int guide_y=text_y+(int)(10u*scale);
        const char *guidance[]={"Hardware keyboard","Type to edit","ENTER done","ESC cancel"};
        for(unsigned i=0;i<4;i++){
            if(guide_y+(int)(7u*scale)>y+h)break;
            text(pad,guide_y,w,guidance[i],scale,fg);guide_y+=(int)(9u*scale);
        }
        return;
    }
    for(unsigned i=0;;i++){
        keyboard_key key;if(!keyboard_bounds((unsigned)n->value,i,&key))break;
        char ch[2];const char *caption=keyboard_caption((unsigned)n->value,key.key,ch);
        unsigned ks=scale;while(ks>1&&strlen(caption)*6u*ks>(unsigned)(key.w>4?key.w-4:0))--ks;
        bool selected=n->id==path.focus[path.depth-1]&&focus_part==i;
        fill(key.x,key.y,key.w,key.h,accent);
        int border=selected?3:1;
        fill(key.x+border,key.y+border,key.w-2*border,key.h-2*border,bg);
        int tw=(int)(strlen(caption)*6u*ks)- (int)ks;
        text(key.x+(key.w-tw)/2,key.y+(key.h-(int)(7u*ks))/2,key.w,caption,ks,fg);
        add_hit(key.x,key.y,key.w,key.h,HIT_VALUE,n->id,0,(int32_t)key.key);
    }
}
static void draw_node(const risc_scene_node_v1 *n,int y,int h){
    if(n->kind==RISC_SCENE_KEYBOARD_NODE){draw_keyboard(n,y,h);return;}
    int pad=(int)profile->padding,x=pad,w=(int)width-2*pad;
    unsigned scale=profile->font_scale;uint16_t fg=(uint16_t)profile->foreground_rgb565;
    if(n->kind==RISC_SCENE_ACTION||n->kind==RISC_SCENE_LINK){button(x,y+2,w,h-4,n->label,n->kind==RISC_SCENE_LINK?HIT_LINK:HIT_ACTION,n,0);return;}
    if(n->kind==RISC_SCENE_TEXT_NODE){
        unsigned lines=wrapped_lines(n->label,(unsigned)w/(6u*scale));
        wrapped_text(x,y+4,w,n->label,scale,fg);
        wrapped_text(x,y+(int)(lines*8u*scale)+7,w,n->text,scale,fg);return;
    }
    text(x,y+4,n->kind==RISC_SCENE_BOOLEAN?w/2-6:w,n->label,scale,fg);
    if(n->kind==RISC_SCENE_BOOLEAN){button(x+w/2,y+2,w/2,h-4,n->value?"ON":"OFF",HIT_VALUE,n,!n->value);return;}
    char value[24];
    if(n->kind==RISC_SCENE_TIME_OF_DAY)snprintf(value,sizeof(value),"%02u:%02u",(unsigned)n->value/60u,(unsigned)n->value%60u);
    else snprintf(value,sizeof(value),"%ld",(long)n->value);
    unsigned vs=scale+1;int vy=y+(int)(8*scale)+7;
    text(x,vy,w,value,vs,fg);
    int by=y+h-(int)(7*scale)-12,bh=(int)(7*scale)+10;
    if(n->kind==RISC_SCENE_TIME_OF_DAY){
        const char *labels[]={"-H","+H","-M","+M"};
        for(unsigned i=0;i<4;i++)button(x+(int)i*w/4,by,w/4-3,bh,labels[i],HIT_VALUE,n,changed_value(n,(i&1)?1:-1,i<2?60:1));
        if(n->id==path.focus[path.depth-1])fill(x+(focus_part?3:0)*(int)(6*vs),vy+(int)(7*vs)+1,(int)(11*vs),2,(uint16_t)profile->accent_rgb565);
    }else {
        button(x,by,w/2-3,bh,"-",HIT_VALUE,n,changed_value(n,-1,n->step));
        button(x+w/2,by,w/2-3,bh,"+",HIT_VALUE,n,changed_value(n,1,n->step));
    }
}
static bool valid_surface(void){
    if(!surface.frame||!surface.pixels||surface.width!=info.width||surface.height!=info.height)return false;
    uint32_t bits=surface.pixel_format==1?1:surface.pixel_format==2?2:surface.pixel_format==3?4:surface.pixel_format==4?8:surface.pixel_format==5?16:0;
    if(!bits)return false;
    uint64_t row=((uint64_t)surface.width*bits+7)/8;
    return surface.stride_bytes>=row&&(uint64_t)surface.stride_bytes*surface.height<=surface.size_bytes;
}
static int32_t paint(void){
    uint32_t format=profile->preferred_format;
    if(!(info.supported_formats&RISC_DISPLAY_FORMAT_BIT(format)))format=info.preferred_format;
    surface=(risc_display_surface_v1){0};
    bool ok=display->acquire(display->context,format,&surface);
    if(!alive())return RISC_SCENE_RETAINED;
    if(!ok)return RISC_SCENE_AGAIN;
    if(!valid_surface()||surface.pixel_format!=format)return fail_retained();
    fill(0,0,(int)width,(int)height,(uint16_t)profile->background_rgb565);
    upload=(hit_map){.revision=document.revision,.route=current_route(),.epoch=epoch,.keyboard=current_keyboard()!=NULL};
    int top=(int)top_height(),bottom=(int)bottom_height(),pad=(int)profile->padding;
    int header_button=(int)(profile->font_scale*30u+8u);
    button(pad,3,header_button,top-6,"BACK",HIT_BACK,NULL,0);
    button((int)width-pad-header_button,3,header_button,top-6,"HOME",HIT_HOME,NULL,0);
    int r=route_index(&document,current_route());
    if(r>=0)text(pad+header_button+6,(top-(int)(7*profile->font_scale))/2,
                 (int)width-2*(pad+header_button+6),document.routes[r].title,profile->font_scale,(uint16_t)profile->foreground_rgb565);
    unsigned list[RISC_SCENE_MAX_NODES],starts[RISC_SCENE_MAX_NODES+1],count;
    unsigned pages=build_pages(list,starts,&count);if(page>=pages)page=pages-1;
    int y=top;
    for(unsigned i=starts[page];i<starts[page+1];i++){
        const risc_scene_node_v1 *n=&document.nodes[list[i]];int h=(int)row_height(n);
        if(y+h>(int)height-bottom)h=(int)height-bottom-y;
        if(h>0)draw_node(n,y,h);
        y+=h;
    }
    if(pages>1){
        int y0=(int)height-bottom+2,w=((int)width-2*pad)/3;
        if(page)button(pad,y0,w-2,bottom-4,"PREV",HIT_PREVIOUS,NULL,0);
        char label[24];snprintf(label,sizeof(label),"%u/%u",page+1,pages);
        text(pad+w+3,y0+(bottom-(int)(7*profile->font_scale))/2,w-6,label,profile->font_scale,(uint16_t)profile->foreground_rgb565);
        if(page+1<pages)button(pad+2*w,y0,w-2,bottom-4,"NEXT",HIT_NEXT,NULL,0);
    }
    risc_display_present_options_v1 options={RISC_DISPLAY_PRESENT_DEFAULT,RISC_DISPLAY_QUEUE_FIFO,0};
    if(upload.keyboard){
        options.intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;
        if(info.flags&RISC_DISPLAY_INFO_MAILBOX)options.queue_policy=RISC_DISPLAY_QUEUE_MAILBOX;
    }
    present_token=0;
    ok=display->submit(display->context,surface.frame,NULL,0,&options,&present_token);
    if(!alive()||!ok)return fail_retained();
    /* Successful submit transfers the frame to the display provider. The
     * present token is the only remaining custody; this frame is not released
     * by the presenter, including after completion or supersession. */
    surface=(risc_display_surface_v1){0};
    if(!present_token)return fail_retained();
    pending=true;dirty=false;return RISC_SCENE_OK;
}
static int32_t complete_frame(void){
    if(!pending)return RISC_SCENE_OK;
    risc_display_present_status_v1 s={0};
    bool ok=display->present_status(display->context,present_token,&s);
    if(!alive()||!ok)return fail_retained();
    if(s.state==RISC_DISPLAY_PRESENT_QUEUED||s.state==RISC_DISPLAY_PRESENT_ACTIVE)return RISC_SCENE_AGAIN;
    if(s.state!=RISC_DISPLAY_PRESENT_COMPLETE&&s.state!=RISC_DISPLAY_PRESENT_SUPERSEDED)return fail_retained();
    if(s.state==RISC_DISPLAY_PRESENT_COMPLETE){
        visible=upload;
        /* Events gathered before this keyboard image became visible are not
         * keys for the new image. Drop them and rearm only from neutral input. */
        if(upload.keyboard&&!closing&&view_current()){
            if(!synchronize_touch())return fail_retained();
            if(navigation){bool ok=navigation->reset(navigation->context);if(!alive()||!ok)return fail_retained();}
        }
    }
    else dirty=true;
    pending=false;present_token=0;
    return RISC_SCENE_OK;
}
static int32_t close_owned(void){
    closing=true;have_event=false;contact=false;
    int32_t r=complete_frame();if(r!=RISC_SCENE_OK)return r;
    if(subscription){bool ok=touch->unsubscribe(touch->context,subscription);if(!alive()||!ok)return fail_retained();subscription=0;}
    if(nav_claimed){
        bool ok=navigation->foreground(navigation->context,NULL,0);if(!alive()||!ok)return fail_retained();
        ok=navigation->reset(navigation->context);if(!alive()||!ok)return fail_retained();
        nav_claimed=false;
    }
    active=false;closing=false;session=0;dirty=false;visible=(hit_map){0};upload=(hit_map){0};
    memset(&document,0,sizeof(document));memset(&path,0,sizeof(path));return RISC_SCENE_OK;
}
static int32_t scene_open(void *c,const risc_scene_document_v1 *d,const risc_scene_navigation_v1 *p,uint64_t *out){
    (void)c;if(!alive())return RISC_SCENE_RETAINED;
    if(!started||!out||!valid_document(d)||(p&&!valid_path(d,p)))return RISC_SCENE_INVALID;
    if(active)return RISC_SCENE_BUSY;
    if(serial==UINT64_MAX)return RISC_SCENE_UNAVAILABLE;
    info=(risc_display_info_v1){.api_version=1,.struct_size=sizeof(info)};
    bool ok=display->get_info(display->context,&info);
    if(!alive())return RISC_SCENE_RETAINED;
    if(!ok||info.api_version!=1||info.struct_size<sizeof(info)||
       info.width<160||info.height<160||info.width>2048||info.height>2048||
       info.preferred_format<1||info.preferred_format>5||
       !(info.supported_formats&RISC_DISPLAY_FORMAT_BIT(info.preferred_format)))return RISC_SCENE_UNAVAILABLE;
    width=info.width;height=info.height;
    if(profile->display_rotation==90||profile->display_rotation==270){width=info.height;height=info.width;}
    if(height<=top_height()+bottom_height()+profile->row_height)return RISC_SCENE_UNAVAILABLE;
    document=*d;path=(risc_scene_navigation_v1){.api_version=1,.struct_size=sizeof(path),.depth=1,.routes={d->root}};
    if(p)path=*p;
    active=true;session=++serial;epoch=1;event_serial=0;closing=false;dirty=true;pending=false;have_event=false;visible=(hit_map){0};focus_part=0;keyboard_waiting=false;
    restore_page();subscription=touch->subscribe(touch->context);
    if(!alive())return RISC_SCENE_RETAINED;
    if(!subscription||!synchronize_touch()){
        if(retained)return RISC_SCENE_RETAINED;
        int32_t r=close_owned();return r==RISC_SCENE_OK?RISC_SCENE_UNAVAILABLE:r;
    }
    if(navigation){
        const risc_input_foreground_v1 claims[]={{"input.touch.raw",1}};nav_claimed=true;
        ok=navigation->foreground(navigation->context,claims,1);
        if(!alive())return RISC_SCENE_RETAINED;
        if(ok){ok=navigation->reset(navigation->context);if(!alive())return RISC_SCENE_RETAINED;}
        if(!ok){
            int32_t r=close_owned();return r==RISC_SCENE_OK?RISC_SCENE_UNAVAILABLE:r;
        }
    }
    *out=session;return RISC_SCENE_OK;
}
static int32_t scene_update(void *c,uint64_t s,const risc_scene_document_v1 *d){
    (void)c;int32_t r=check(s);if(r)return r;if(closing)return RISC_SCENE_BUSY;
    if(!valid_document(d)||d->root!=document.root||!valid_path(d,&path))return RISC_SCENE_INVALID;
    if(d->revision<=document.revision)return RISC_SCENE_STALE;
    const risc_scene_node_v1 *old=current_keyboard();
    if(old){
        int next=node_index(d,old->id);
        if(next<0||d->nodes[next].kind!=old->kind||d->nodes[next].value!=old->value||d->nodes[next].flags!=old->flags)focus_part=0;
    }
    document=*d;have_event=false;dirty=true;contact=false;neutral=false;keyboard_waiting=false;restore_page();return RISC_SCENE_OK;
}
static int32_t scene_next(void *c,uint64_t s,risc_scene_event_v1 *event){
    (void)c;int32_t r=check(s);if(r)return r;
    if(!event||event->struct_size!=sizeof(*event))return RISC_SCENE_INVALID;
    if(closing)return RISC_SCENE_BUSY;
    /* Drain keyboard input before observing a pending frame's completion.
     * Physical providers may discover an old tap only when polled; sampling
     * their snapshot first could relabel that tap as input for the new image. */
    bool keyboard_pending=pending&&current_keyboard()!=NULL;
    if(!keyboard_pending){r=complete_frame();if(r<0)return r;}
    r=poll_touch();if(r<0)return r;
    r=poll_navigation();if(r<0)return r;
    if(retained)return RISC_SCENE_RETAINED;
    if(keyboard_pending){r=complete_frame();if(r<0)return r;}
    if(retained)return RISC_SCENE_RETAINED;
    if(dirty&&!pending){r=paint();if(r<0)return r;}
    if(have_event){
        int i=node_index(&document,queued.node);
        if(i>=0&&document.nodes[i].kind==RISC_SCENE_KEYBOARD_NODE)keyboard_waiting=true;
        *event=queued;have_event=false;return RISC_SCENE_OK;
    }
    return RISC_SCENE_IDLE;
}
static int32_t scene_navigate(void *c,uint64_t s,uint32_t op,uint32_t route){
    (void)c;int32_t r=check(s);if(r)return r;if(closing)return RISC_SCENE_BUSY;return navigate_impl(op,route);
}
static int32_t scene_snapshot(void *c,uint64_t s,risc_scene_navigation_v1 *p,uint32_t *flags){
    (void)c;int32_t r=check(s);if(r)return r;
    if(!p||p->struct_size!=sizeof(*p)||!flags)return RISC_SCENE_INVALID;
    *p=path;*flags=(pending||dirty?RISC_SCENE_PRESENTING:0u)|(closing?RISC_SCENE_CLOSING:0u);return RISC_SCENE_OK;
}
static int32_t scene_close(void *c,uint64_t s){(void)c;int32_t r=check(s);return r?r:close_owned();}
static const risc_scene_api_v1 api={1,sizeof(api),NULL,scene_open,scene_update,scene_next,scene_navigate,scene_snapshot,scene_close};
static bool start(const risc_provider_dependency_v1 *deps,size_t count){
    if(started||active||retained||count<4||count>5||!deps)return false;
    display=NULL;touch=NULL;navigation=NULL;profile=NULL;clock_api=NULL;
    for(size_t i=0;i<count;i++){
        if(!deps[i].capability_id||deps[i].api_version!=1||!deps[i].api)return false;
        for(size_t j=0;j<i;j++)if(!strcmp(deps[j].capability_id,deps[i].capability_id))return false;
        if(!strcmp(deps[i].capability_id,RISC_DISPLAY_OUTPUT_CAPABILITY))display=deps[i].api;
        else if(!strcmp(deps[i].capability_id,"input.touch.raw"))touch=deps[i].api;
        else if(!strcmp(deps[i].capability_id,"input.navigation"))navigation=deps[i].api;
        else if(!strcmp(deps[i].capability_id,"platform.clock"))clock_api=deps[i].api;
        else if(!strcmp(deps[i].capability_id,RISC_SCENE_PROFILE_CAPABILITY))profile=deps[i].api;
        else return false;
    }
    if(!display||display->api_version!=1||display->struct_size<sizeof(*display)||!display->get_info||!display->acquire||!display->release||!display->submit||!display->present_status||
       !touch||touch->api_version!=1||touch->struct_size<sizeof(*touch)||!touch->subscribe||!touch->unsubscribe||!touch->poll||!touch->next||!touch->snapshot||
       !clock_api||clock_api->api_version!=1||clock_api->struct_size<sizeof(*clock_api)||!clock_api->monotonic_ms||
       !profile||profile->api_version!=1||profile->struct_size!=sizeof(*profile)||profile->preferred_format<1||profile->preferred_format>5||
       profile->font_scale<1||profile->font_scale>4||profile->row_height<36||profile->row_height<profile->font_scale*12u+18u||profile->row_height>160||profile->padding>32||profile->reserved||
       profile->foreground_rgb565>65535||profile->background_rgb565>65535||profile->accent_rgb565>65535||
       (profile->display_rotation!=0&&profile->display_rotation!=90&&profile->display_rotation!=180&&profile->display_rotation!=270)||
       (profile->touch_rotation!=0&&profile->touch_rotation!=90&&profile->touch_rotation!=180&&profile->touch_rotation!=270))return false;
    if(navigation&&(navigation->api_version!=1||navigation->struct_size<sizeof(*navigation)||!navigation->poll||!navigation->foreground||!navigation->reset))return false;
    started=true;return true;
}
static bool quiesce(void){return !retained&&!active&&!pending&&!subscription&&!nav_claimed;}
static void stop(void){if(!quiesce())return;started=false;display=NULL;touch=NULL;navigation=NULL;clock_api=NULL;profile=NULL;}
static const risc_driver_v2 driver={2,sizeof(driver),"scene-host",RISC_SCENE_CAPABILITY,1,&api,start,stop,quiesce};
__attribute__((visibility("default"))) const risc_driver_v2 *t5_driver_get(uint32_t abi){return abi==2?&driver:NULL;}

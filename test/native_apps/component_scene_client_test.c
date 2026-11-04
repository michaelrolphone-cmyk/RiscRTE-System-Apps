/* Real scene client with adversarial capability endpoints; no hardware proof. */
#include <assert.h>
#include <stdio.h>
#include "RiscComponentSceneClient.h"
static risc_component_scene_client ui;
static bool live,absent,dirty_acquire,dirty_release,invalidate,reenter,dirty_idle;
static unsigned retains,configs,opens,updates,closes,releases;
static int open_rc,close_rc,next_rc;
static uint32_t flags;
static risc_scene_event_v1 event;
static risc_scene_components_api_v1 api;
static risc_components_document_v1 doc;
static bool alive(void*p){(void)p;return live;}
static void retain(void*p){(void)p;retains++;live=false;}
static void callback(void){if(invalidate)live=false;if(reenter){reenter=false;(void)risc_component_scene_snapshot(&ui);}}
static bool acquire(const char*n,uint32_t v,uint64_t i,risc_runtime_capability_v1*g){assert(!strcmp(n,"ui.scene")&&v==1&&i==0);if(!absent||dirty_acquire){g->api=&api;g->slot=1;g->generation=1;}callback();return !absent;}
static bool release(risc_runtime_capability_v1*g){releases++;if(!dirty_release)*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};callback();return true;}
static int snapshot(void*p,uint64_t t,risc_scene_navigation_v1*n,uint32_t*f){(void)p;assert(t==7);n->depth=1;n->routes[0]=1;*f=flags;callback();return RISC_SCENE_OK;}
static int next(void*p,uint64_t t,risc_scene_event_v1*e){(void)p;assert(t==7);if(next_rc==RISC_SCENE_OK)*e=event;else if(dirty_idle)e->action=99;callback();return next_rc;}
static int close_scene(void*p,uint64_t t){(void)p;assert(t==7);closes++;callback();return close_rc;}
static int configure(void*p,uint64_t t,uint32_t f){(void)p;assert(t==7);assert(f<=1);configs++;callback();return 0;}
static int open_scene(void*p,const risc_components_document_v1*d,const risc_scene_navigation_v1*n,uint64_t*t){(void)p;(void)n;assert(d->revision);opens++;if(open_rc==0)*t=7;callback();return open_rc;}
static int update(void*p,uint64_t t,const risc_components_document_v1*d){(void)p;assert(t==7&&d->revision);updates++;callback();return 0;}
static const risc_runtime_api_v1 rt={.api_version=1,.struct_size=sizeof(rt),.acquire=acquire,.release=release};
static void reset(void){live=true;absent=dirty_acquire=dirty_release=invalidate=reenter=dirty_idle=false;retains=configs=opens=updates=closes=releases=0;flags=0;open_rc=close_rc=0;next_rc=RISC_SCENE_IDLE;api=(risc_scene_components_api_v1){.lifecycle={.base={.api_version=1,.struct_size=sizeof(api),.snapshot=snapshot,.next=next,.close=close_scene},.configure=configure},.tag=RISC_COMPONENTS_TAG,.version=1,.open=open_scene,.update=update};doc=(risc_components_document_v1){.api_version=1,.struct_size=sizeof(doc),.revision=1,.root=1,.route_count=1,.routes={{.id=1}}};risc_component_scene_init(&ui,&rt,alive,retain,NULL);}
static void opened(void){assert(risc_component_scene_bind(&ui)==1);assert(risc_component_scene_publish(&ui,&doc,false)==1);}
int main(void){
 reset();opened();risc_scene_event_v1 out={0};assert(risc_component_scene_next(&ui,&out)==0&&live&&!retains);doc.revision++;assert(risc_component_scene_publish(&ui,&doc,false)==1&&configs==1&&updates==1);doc.revision++;assert(risc_component_scene_publish(&ui,&doc,true)==1&&configs==2);assert(risc_component_scene_close(&ui));doc.revision++;assert(risc_component_scene_publish(&ui,&doc,true)==1&&configs==3);assert(risc_component_scene_release(&ui)&&releases==1);
 reset();opened();dirty_idle=true;assert(risc_component_scene_next(&ui,&out)==-1&&retains==1);
 reset();opened();next_rc=RISC_SCENE_AGAIN;assert(risc_component_scene_next(&ui,&out)==-1&&retains==1);
 reset();opened();close_rc=RISC_SCENE_AGAIN;assert(!risc_component_scene_release(&ui)&&ui.token==7&&!releases);doc.revision++;assert(risc_component_scene_publish(&ui,&doc,false)==0);close_rc=0;assert(risc_component_scene_release(&ui)&&releases==1);
 reset();absent=true;assert(risc_component_scene_bind(&ui)==0&&live&&risc_component_scene_release(&ui));
 reset();absent=dirty_acquire=true;assert(risc_component_scene_bind(&ui)==-1&&retains==1);
 reset();opened();dirty_release=true;assert(!risc_component_scene_release(&ui)&&retains==1);
 reset();opened();invalidate=true;assert(!risc_component_scene_snapshot(&ui)&&ui.terminal);assert(!risc_component_scene_release(&ui)&&!releases);
 reset();opened();reenter=true;assert(!risc_component_scene_snapshot(&ui)&&retains==1);
 reset();opened();flags=RISC_SCENE_PRESENTING;doc.revision++;assert(risc_component_scene_publish(&ui,&doc,false)==0&&!updates);flags=RISC_SCENE_INPUT_BUSY;assert(risc_component_scene_publish(&ui,&doc,false)==0&&!updates);flags=16;assert(!risc_component_scene_snapshot(&ui)&&retains==1);
 reset();opened();next_rc=0;event=(risc_scene_event_v1){.struct_size=sizeof(event),.sequence=1,.document_revision=1,.kind=RISC_SCENE_ACTION_EVENT,.action=1};assert(risc_component_scene_next(&ui,&out)==1);assert(risc_component_scene_next(&ui,&out)==-1&&retains==1);
 reset();assert(risc_component_scene_bind(&ui)==1);open_rc=RISC_SCENE_AGAIN;assert(risc_component_scene_publish(&ui,&doc,false)==0&&!ui.token&&live);open_rc=0;assert(risc_component_scene_publish(&ui,&doc,false)==1);assert(risc_component_scene_release(&ui));
 reset();assert(risc_component_scene_presented(&ui,1)==0);opened();
 assert(risc_component_scene_presented(&ui,1)==1);
 assert(risc_component_scene_presented(&ui,0)==0&&risc_component_scene_presented(&ui,2)==0);
 flags=RISC_SCENE_PRESENTING;assert(risc_component_scene_presented(&ui,1)==0);
 flags=RISC_SCENE_INPUT_BUSY;assert(risc_component_scene_presented(&ui,1)==0);
 flags=RISC_SCENE_CLOSING;assert(risc_component_scene_presented(&ui,1)==0);
 flags=RISC_SCENE_ACTIVITY;assert(risc_component_scene_presented(&ui,1)==1);
 flags=0;doc.revision=2;assert(risc_component_scene_publish(&ui,&doc,false)==1);
 assert(risc_component_scene_presented(&ui,1)==0&&risc_component_scene_presented(&ui,2)==1);
 invalidate=true;assert(risc_component_scene_presented(&ui,2)==-1);
 assert(risc_component_scene_presented(&ui,2)==-1&&!releases);
 puts("13 shared component scene-client groups PASS (endpoint doubles)");
}

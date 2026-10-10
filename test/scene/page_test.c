#define main baseline_main
#include "host_test.c"
#undef main
#include "RiscScenePageV1.h"
int main(void){
 w=480;h=800;format=1;prof=(risc_scene_profile_v1){1,sizeof(prof),1,3,88,20,0,0xffff,0,0,0,0};
 startup();const risc_scene_page_api_v1 *page=risc_scene_page_get_v1(api);assert(page);
 risc_scene_page_geometry_v1 g={.struct_size=sizeof(g)};assert(!page->geometry(NULL,&g));assert(g.width==480&&g.height>500&&g.stride==60);
 size_t bytes=(size_t)g.stride*g.height;uint8_t *bitmap=malloc(bytes);memset(bitmap,0xff,bytes);
 risc_scene_page_document_v1 p={.struct_size=sizeof(p),.revision=2,.previous_action=71,.menu_action=72,.next_action=73,.back_action=72};strcpy(p.title,"PORTABLE READER");strcpy(p.footer,"CHAPTER 1");
 assert(page->present_page(NULL,session,&p,bitmap,bytes-1)==RISC_SCENE_INVALID);
 expected_intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;assert(!page->present_page(NULL,session,&p,bitmap,bytes));memset(bitmap,0,bytes);free(bitmap);settle();
 /* The client buffer was white, then overwritten/freed. The middle of the
    published body must remain white: caller pixels are copied synchronously. */
 for(unsigned y=300;y<400;y++)for(unsigned x=0;x<g.stride;x++)assert(pixels[16+y*g.stride+x]==0);
 assert(page->present_page(NULL,session,&p,pixels,bytes)==RISC_SCENE_INVALID);
 risc_scene_event_v1 e;nav_pressed=RISC_NAV_RIGHT;assert(tick(&e)==0&&e.action==73&&e.document_revision==2);
 nav_pressed=RISC_NAV_LEFT;assert(tick(&e)==0&&e.action==71);
 /* New component document leaves page mode and old page intents behind. */
 risc_components_document_v1 c={.api_version=1,.struct_size=sizeof(c),.revision=3,.root=1,.route_count=1,.node_count=1,.screen_key=2};c.routes[0]=(risc_scene_route_v1){1,99,"LIBRARY"};c.nodes[0]=(risc_scene_node_v1){.id=1,.route=1,.kind=RISC_COMPONENT_ROW,.action=90};strcpy(c.nodes[0].label,"A BOOK");expected_intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;assert(!page->components.update(NULL,session,&c));settle();
 finish();puts("Copied page bitmap, navigation intents, revision validation and component return PASS");return 0;
}

#define main baseline_main
#include "host_test.c"
#undef main
#include "RiscSceneReadingV1.h"
static FILE *frames;
static void capture(void){settle();assert(fwrite(pixels+16,1,240*240*2,frames)==240*240*2);}
int main(int argc,char**argv){
 assert(argc==2);frames=fopen(argv[1],"wb");assert(frames);startup();
 const risc_scene_reading_api_v1*r=risc_scene_reading_get_v1(api);assert(r);
 risc_scene_page_geometry_v1 g={.struct_size=sizeof(g)};assert(!r->reading_geometry(NULL,&g));
 size_t length=(size_t)g.stride*g.height;uint8_t*b=malloc(length);for(size_t i=0;i<length;i++)b[i]=(uint8_t)(i*17);
 risc_scene_reading_document_v1 d={.struct_size=sizeof(d),.base={.struct_size=sizeof(d.base),.revision=2,.previous_action=71,.menu_action=72,.next_action=73,.back_action=74},.bookmark_action=75,.layout_action=76,.contents_action=77,.scrub_action=78,.scroll_action=79,.progress=450};strcpy(d.base.title,"READING");strcpy(d.base.footer,"1 / 20");
 assert(!r->present_reading(NULL,session,&d,b,length,1));expected_intent=RISC_DISPLAY_PRESENT_CLEAN;capture();
 d.flags=RISC_SCENE_READING_CHROME;d.base.revision++;assert(!r->present_reading(NULL,session,&d,NULL,0,0));expected_intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;capture();
 assert(!api->close(NULL,session));
 const risc_scene_components_api_v1*c=risc_scene_components_get_v1(api);
 risc_components_document_v1 rich={.api_version=1,.struct_size=sizeof(rich),.revision=1,.root=1,.route_count=1,.node_count=3,.screen_key=1,.routes={{.id=1,.title="COMPONENTS"}},.nodes={{.id=1,.route=1,.kind=RISC_COMPONENT_SECTION,.label="Section"},{.id=2,.route=1,.kind=RISC_SCENE_TEXT_NODE,.label="Body label",.text="Small detail"},{.id=3,.route=1,.kind=RISC_SCENE_ACTION,.action=1,.label="Action"}}};
 assert(!c->open(NULL,&rich,NULL,&session));capture();finish();free(b);fclose(frames);
#if SCENE_HOST_WATCH_240
 driver=t5_driver_get(2);risc_provider_dependency_v1 deps[]={{"display.output",1,&disp},{"input.touch.raw",1,&raw},{"platform.clock",1,&clk},{"ui.presentation-profile",1,&prof}};
 assert(driver->start(deps,4));make_document();w=480;h=480;unsigned before=calls;assert(api->open(NULL,&doc,NULL,&session)==RISC_SCENE_UNAVAILABLE);assert(calls==before+1&&!subscribed&&driver->quiesce());driver->stop();
#endif
 puts("Watch240 component/reading raster fixture and unsupported geometry rejection PASS");
}

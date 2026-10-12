/* Exercise the actual compact API and fail-closed validation through public
 * entry points, using the established display/touch/lifecycle fixture. */
#define main baseline_main
#include "host_test.c"
#undef main
#include "RiscSceneTelemetryV1.h"
int main(void){
 startup();
 assert(api->struct_size==sizeof(risc_scene_reading_links_api_v1));
 assert(risc_scene_reading_links_get_v1(api));
 assert(risc_scene_reading_get_v1(api));
 assert(risc_scene_checkpoint_get_v1(api));
 assert(!risc_scene_telemetry_get_v1(api));
 assert(!api->close(NULL,session));
 const risc_scene_components_api_v1 *components=risc_scene_components_get_v1(api);assert(components);
 risc_components_document_v1 d={.api_version=1,.struct_size=sizeof(d),.revision=1,.root=1,.route_count=1,.node_count=1,.screen_key=1,
  .routes={{.id=1,.title="Legacy components"}},.nodes={{.id=1,.route=1,.kind=RISC_SCENE_TEXT_NODE,.label="Preserved"}}};
 expected_intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;assert(!components->open(NULL,&d,NULL,&session));settle();
 unsigned before=calls;
 risc_components_document_v1 bad=d;bad.revision++;
 bad.flags=RISC_COMPONENTS_TELEMETRY;
 assert(components->update(NULL,session,&bad)==RISC_SCENE_INVALID);
 bad.flags=0;
 for(unsigned k=RISC_COMPONENT_INSTRUMENT;k<=RISC_COMPONENT_CODE_ROW;k++){
  bad.nodes[0].kind=k;assert(components->update(NULL,session,&bad)==RISC_SCENE_INVALID);
 }
 bad.nodes[0].kind=RISC_SCENE_TEXT_NODE;
 for(unsigned k=RISC_SYMBOL_GAUGE;k<=RISC_SYMBOL_GEAR;k++){
  bad.details[0].symbol=k;assert(components->update(NULL,session,&bad)==RISC_SCENE_INVALID);
 }
 assert(calls==before);bad=d;bad.revision++;assert(!components->update(NULL,session,&bad));settle();
 assert(!api->close(NULL,session));
 bad.flags=RISC_COMPONENTS_TELEMETRY;before=calls;
 assert(components->open(NULL,&bad,NULL,&session)==RISC_SCENE_INVALID);assert(calls==before);
 bad.flags=0;assert(!components->open(NULL,&bad,NULL,&session));settle();finish();
 puts("Compact profile: reading ABI preserved; telemetry undiscoverable; optional flag, 11 kinds and 12 icons rejected atomically; legacy reopen PASS");
 return 0;
}

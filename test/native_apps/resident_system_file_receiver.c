/* A minimal admitted receiver proves Files uses Runtime's real file.open
 * copy/return-cookie lifecycle. It neither renders nor touches device APIs. */
#include "RiscRuntimeV1.h"
#include "RiscResidentShellV1.h"
#include "T5FileOpenApi.h"
#include <assert.h>
#include <string.h>
extern void system_event(unsigned,unsigned);
__attribute__((visibility("default"))) const risc_resident_app_descriptor_v1_t risc_resident_app_descriptor_v1={1,sizeof(risc_resident_app_descriptor_v1_t),RISC_RESIDENT_ROLE_FOREGROUND,0};
int app_module_init(void){return 0;}
void app_module_fini(void){}
void app_main(void){
 const risc_runtime_api_v1 *rt=risc_runtime_get_api(1);assert(rt);
 risc_runtime_capability_v1 grant={.struct_size=sizeof(grant)};assert(rt->acquire("file.open",1,0,&grant));
 const t5_file_open_api_v1 *files=grant.api;char path[512];
 assert(files->source_path_get(path,sizeof(path))&&!strcmp(path,"/sd/File 003.txt"));
 assert(rt->release(&grant));system_event(13,1);
}

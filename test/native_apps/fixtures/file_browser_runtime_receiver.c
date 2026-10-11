#include <RiscRuntimeV1.h>
#include <T5FileOpenApi.h>
#include <assert.h>
#include <string.h>
extern void file_browser_runtime_event(const char *name);
extern const char *file_browser_runtime_mode(void);
__attribute__((visibility("default"))) void app_main(void) {
    const risc_runtime_api_v1 *runtime=risc_runtime_get_api(1);assert(runtime);
    risc_runtime_capability_v1 grant={.struct_size=sizeof(grant)};
    assert(runtime->acquire("file.open",1,0,&grant));
    const t5_file_open_api_v1 *api=grant.api;char source[T5_FILE_OPEN_PATH_MAX];
    const char *expected=!strcmp(file_browser_runtime_mode(),"nested-handoff")?"/sd/Books/read.txt":"/sd/read.txt";
    assert(api->source_path_get(source,sizeof(source)) && !strcmp(source,expected));
    assert(!api->open_request(source,"receiver",7));
    assert(runtime->release(&grant));file_browser_runtime_event("receiver");
}

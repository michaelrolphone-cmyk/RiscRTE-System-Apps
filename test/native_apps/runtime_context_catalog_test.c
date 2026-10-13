/* Actual Contexts workflow dispatcher resolves bounded UI keys to native routes. */
#define PORTABLE_APP_LAUNCH_GUARD
#define OPTIONAL_SERVICE_FIXTURE_MAIN previous_optional_main
#include "optional_service_adapter_test.c"
static unsigned mode,workflow_result,workflow_reports,workflow_launches;
static uint64_t publication=1;
static char actual_route[RISC_APP_CATALOG_ROUTE_MAX];
static risc_app_catalog_entry_v1 installed;
static int32_t context_catalog_snapshot(void*c,risc_app_catalog_snapshot_v1*out) {
 (void)c;*out=(risc_app_catalog_snapshot_v1){.struct_size=sizeof(*out),.count=1,.generation=publication};return 0;
}
static int32_t context_catalog_get(void*c,uint64_t generation,uint32_t index,risc_app_catalog_entry_v1*out) {
 (void)c;if(generation!=publication)return RISC_APP_CATALOG_STALE;
 if(index)return RISC_APP_CATALOG_NOT_FOUND;
 *out=installed;return 0;
}
static const risc_app_catalog_v1 context_catalog={1,sizeof(context_catalog),NULL,context_catalog_snapshot,context_catalog_get};
static bool context_catalog_acquire(const char*name,uint32_t version,uint64_t instance,risc_runtime_capability_v1*grant) {
 if(!strcmp(name,RISC_APP_CATALOG_CAPABILITY)){assert(version==1&&!instance);grant->api=&context_catalog;grant->slot=87;grant->generation=1;grants++;return true;}
 return startup_acquire(name,version,instance,grant);
}
bool portable_app_before_launch(const char*route) {
 assert(!strcmp(route,installed.file_name));if(mode==2)publication++;return true;
}
static bool context_catalog_launch(const char*route) {strcpy(actual_route,route);workflow_launches++;return true;}
static bool workflow(void*c,uint32_t op,void*request) {
 (void)c;
 if(op==CONTEXTS_FP_EVALUATE)return true;
 if(op==CONTEXTS_FP_WORKFLOW) {
  contexts_workflow_v1 *action=request;
  if(action->claim){action->available=true;action->step.kind=CR_LAUNCH;if(mode==3)strcpy(action->step.text,"absent.elf");
   else if(mode==4)strcpy(action->step.text,installed.file_name);
   else {strcpy(action->step.text,"id:");strcat(action->step.text,installed.id);}}
  else {workflow_result=action->result;workflow_reports++;}
  return true;
 }
 return startup_fingerprint(c,op,request);
}
int main(int argc,char**argv) {
 assert(argc==2);mode=(unsigned)atoi(argv[1]);configure();
 startup_runtime.acquire=context_catalog_acquire;startup_runtime.request_launch=context_catalog_launch;
 assert(app_module_init()==0);
 installed=(risc_app_catalog_entry_v1){.struct_size=sizeof(installed),.flags=mode==1?0:RISC_APP_CATALOG_AVAILABLE,
  .display_name="Installed workflow",.id="workflow",.icon="solid:f017"};
 strcpy(installed.file_name,"Apps/");for(unsigned i=0;i<20;i++)strcat(installed.file_name,"package-");strcat(installed.file_name,"/workflow.elf");
 if(mode==4)strcpy(installed.file_name,"workflow.elf");
 startup_fp.fingerprint=workflow;contexts_client.api=&startup_fp.base;contexts_client.rules_available=true;
 assert(contexts_rules_tick());assert(workflow_reports==1);
 if(mode==0||mode==4){assert(workflow_launches==1&&workflow_result==CR_RAN);assert(!strcmp(actual_route,installed.file_name));}
 else {assert(!workflow_launches&&workflow_result==CR_FAILED);}
 app_module_fini();assert(!grants&&!subscriptions&&!frame_count&&!fenced);
 printf("Actual Contexts runtime catalog route resolution mode %u PASS\n",mode);return 0;
}

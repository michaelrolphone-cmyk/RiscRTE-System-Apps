/* Production full-feature adapter; provider timing/failures are modeled.
 * The script adds test-only accessors after the unchanged production source. */
#define main initial_brightness_original_main
#include INITIAL_BRIGHTNESS_FIXTURE
#undef main
void brightness_test_paint(void);
bool brightness_test_progress(void);
bool brightness_test_failed(void);
bool brightness_test_preview(unsigned);
bool brightness_test_torch(bool);

static void settle_brightness(void) {
  for(unsigned i=0;i<20&&!brightness_test_failed();++i) {
    ++ticks;
    assert(brightness_test_progress());
  }
}
int main(int argc,char **argv) {
  assert(argc==5);
  fw=(unsigned)atoi(argv[1]);fh=(unsigned)atoi(argv[2]);
  use_async=!strcmp(argv[3],"async");const char *scenario=argv[4];
  extended_contexts=(contexts_fingerprint_service_v1){.base=ctx_api,
    .fingerprint_abi=0x31504643u,.fingerprint=healthy_fingerprint};
  extended_contexts.base.struct_size=sizeof(extended_contexts);
  pitch=fw+3;guarded=malloc((fh*pitch+2)*2);assert(guarded);pixels=guarded+1;
  for(unsigned i=0;i<fh*pitch+2;i++)guarded[i]=0xa55a;
  fixture_brightness=!strcmp(scenario,"retained-lit")?40:0;
  fixture_has_brightness=strcmp(scenario,"unsupported")!=0;
  fixture_fault=!strcmp(scenario,"submit-failed")?1:
    !strcmp(scenario,"completion-failed")?2:!strcmp(scenario,"brightness-failed")?3:0;
  assert(app_module_init()==0);
  assert(!fixture_brightness_calls&&!fixture_completed);
  unsigned expected=40;
  if(!strcmp(scenario,"preview-before")||!strcmp(scenario,"torch-before")) {
    fixture_user_brightness=true;
    if(!strcmp(scenario,"preview-before")){assert(brightness_test_preview(73));expected=73;}
    else {assert(brightness_test_torch(true));expected=100;}
    fixture_user_brightness=false;
  }
  unsigned before=fixture_brightness_calls;
  brightness_test_paint();
  if(use_async&&fixture_fault!=1) {
    assert(fixture_brightness_calls==before&&!fixture_completed);
    for(unsigned i=0;i<20&&!brightness_test_failed()&&!fixture_completed;++i) {
      ++ticks;(void)brightness_test_progress();
    }
  }
  if(fixture_fault) {
    assert(brightness_test_failed());
    assert(fixture_brightness_calls==(fixture_fault==3?1u:0u));
    assert(!fixture_brightness);
    if(fixture_fault==2&&use_async)assert(fixture_retained);
    puts("Initial brightness: failed submit/completion/unblank never falsely exposes a frame PASS");
    return 0; /* An uncertain failed completion retains its test-owned buffers. */
  }
  assert(fixture_completed);
  assert(fixture_brightness_calls==before+(fixture_has_brightness?1u:0u));
  assert(fixture_brightness==(fixture_has_brightness?expected:0));
  before=fixture_brightness_calls;
  brightness_test_paint();settle_brightness();assert(fixture_brightness_calls==before);
  if(fixture_has_brightness) {
    /* Apply actual Quick actions; later completions must not restore stale state. */
    fixture_user_brightness=true;
    assert(brightness_test_torch(false));
    assert(brightness_test_preview(68));assert(fixture_brightness==68);
    fixture_user_brightness=false;
    brightness_test_paint();settle_brightness();assert(fixture_brightness==68);
    fixture_user_brightness=true;assert(brightness_test_torch(true));
    fixture_user_brightness=false;
    brightness_test_paint();settle_brightness();assert(fixture_brightness==100);
    fixture_user_brightness=true;assert(brightness_test_torch(false));
    fixture_user_brightness=false;
    brightness_test_paint();settle_brightness();assert(fixture_brightness==40);
  }
  app_module_fini();assert(!grants&&!frames&&!subscriptions);
  /* A second invocation on the same retained provider re-arms exactly once. */
  before=fixture_brightness_calls;fixture_completed=0;
  assert(app_module_init()==0);assert(fixture_brightness_calls==before);
  brightness_test_paint();settle_brightness();
  assert(fixture_brightness_calls==before+(fixture_has_brightness?1u:0u));
  app_module_fini();free(guarded);assert(!grants&&!frames&&!subscriptions&&!allocation_count);
  printf("Initial brightness %ux%u %s %s: first completed frame only, once per invocation, Quick preview/torch preserved PASS\n",
    fw,fh,argv[3],scenario);
  return 0;
}

#define main legacy_fixture_main
#include "portable_handoff_test.c"
#undef main
int main(void) {
 begin_test();assert(app_module_init()==0);assert(springboard_presentation_get());
 draw_fresh();assert(handoff_started==0 && handoff_active);
 mock_ms=20;present(false);assert(memcmp(mock_pixels,mock_old,480));
 assert(mock_presents==1 && handoff_active); /* First transfer already blends. */
 mock_ms=60;draw_fresh();present(false);assert_fresh();assert(!handoff_active);
 assert(mock_presents==2);end_test();
 begin_test();assert(app_module_init()==0);
 t5_app_input_t in={0};assert(poll(&in,20));
 mock_down=true;mock_x=20;mock_y=18;assert(poll(&in,20));assert(!mock_launches);
 mock_down=false;assert(poll(&in,20));assert(in.exit_requested && mock_launches==1);
 end_test();assert(mock_launches==1); /* Fini never replaces an explicit launch. */
 begin_test();assert(app_module_init()==0);failed=true;assert(!poll(&in,20));assert(!mock_launches);end_test();
 puts("Eager handoff: initial paint counted, first transfer blends, sharp by60ms; deliberate Back returns once, failures never synthesize a return");
}

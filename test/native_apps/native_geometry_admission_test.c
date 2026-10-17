/* Complete init/fini path for retained and extended adapter profiles. */
#include <stdint.h>
static unsigned geometry_width,geometry_height,geometry_stride,geometry_size;
#define main inherited_handoff_main
#include FRAME_FIXTURE
#undef main
int main(void) {
 mock_pixels=mock_old; /* No frame is acquired by these admission-only cases. */
 const struct {unsigned w,h;bool admitted[3];} cases[]={
  {0,240,{false,false,false}},{240,0,{false,false,false}},
  {1,1,{true,false,true}},{128,64,{true,false,true}},
  {240,240,{true,true,true}},{410,502,{true,false,true}},
  {502,410,{true,false,true}},{1025,256,{true,false,true}},
  {256,2049,{true,false,true}},{4096,16,{true,false,true}},
  {65535,1,{true,false,true}},{240,65535,{true,false,true}},
  {1,273,{true,false,true}},{1,274,{true,false,false}},
  {1,65535,{true,false,false}},{65535,65535,{false,false,false}},
  {65536,1,{false,false,false}},{1,65536,{false,false,false}}};
 for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++){
  geometry_width=cases[i].w;geometry_height=cases[i].h;mock_flags=RISC_DISPLAY_INFO_ASYNC_PRESENT;
  int result=app_module_init();assert((result==0)==cases[i].admitted[TEST_PROFILE]);
#if TEST_PROFILE == 2
  if(result==0){
   uint16_t lx=0,ly=0;
   assert(nu_height()>0);assert(watch_layout_touch(&nu_layout,geometry_width-1,geometry_height-1,&lx,&ly));
   assert(lx==(geometry_width-1)*240u/geometry_width);
   assert(ly==(geometry_height-1)*240u/geometry_width);
   watch_layout_rect logical={lx,ly,1,1},native;
   assert(watch_layout_native_rect(&nu_layout,&logical,&native));
   assert(watch_layout_contains(&native,geometry_width-1,geometry_height-1));
   assert(!watch_layout_touch(&nu_layout,geometry_width,0,&lx,&ly));
   assert(!watch_layout_touch(&nu_layout,0,geometry_height,&lx,&ly));
  }else if(portable_display_dimensions(geometry_width,geometry_height)&&geometry_width==1){
   assert(nu_height()==0); /* An unrepresentable logical input grid is rejected. */
  }
#endif
  assert(!mock_frames&&!mock_live);app_module_fini();
  assert(!mock_grants&&!mock_subscriptions&&!mock_frames&&!mock_live);
 }
 printf("profile %u: 18 complete adapter init/fini and native input-edge cases PASS\n",TEST_PROFILE);
 return 0;
}

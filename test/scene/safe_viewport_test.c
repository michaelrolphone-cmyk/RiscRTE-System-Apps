#define main historical_fixture_main
#include HOST_FIXTURE
#undef main
static risc_display_insets_v1 insets;
static bool inset_info(void*c,risc_display_info_v1*out){bool ok=get_info(c,out);out->safe_insets=insets;return ok;}
static void depth(unsigned expected){risc_scene_navigation_v1 s={.struct_size=sizeof(s)};uint32_t f;assert(!api->snapshot(NULL,session,&s,&f)&&s.depth==expected);}
int main(int argc,char**argv){
 assert(argc==5);unsigned rotation=(unsigned)strtoul(argv[1],NULL,10);w=(unsigned)strtoul(argv[2],NULL,10);h=(unsigned)strtoul(argv[3],NULL,10);bool invalid=!strcmp(argv[4],"invalid");bool round=!strcmp(argv[4],"round"),narrow=!strcmp(argv[4],"narrow");
 insets=round?(risc_display_insets_v1){69,69,69,69}:(risc_display_insets_v1){29,17,37,13};if(invalid)insets.left=w;else if(narrow)insets.left=w-insets.right-1;
 risc_display_output_api_v1 display=disp;display.get_info=inset_info;prof.display_rotation=rotation;prof.touch_rotation=(360-rotation)%360;
 driver=t5_driver_get(2);api=driver->capability;risc_provider_dependency_v1 deps[]={{"display.output",1,&display},{"input.touch.raw",1,&raw},{"platform.clock",1,&clk},{"ui.presentation-profile",1,&prof}};
 assert(driver->start(deps,4));make_document();int rc=api->open(NULL,&doc,NULL,&session);
 if(invalid||narrow){assert(rc==RISC_SCENE_UNAVAILABLE&&!session&&!subscribed&&!held);driver->stop();puts("invalid inset clean refusal");return 0;}
 assert(rc==0);settle();unsigned ink=0;
 for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){uint16_t p=(uint16_t)pixels[16+(y*w+x)*2]|(uint16_t)pixels[17+(y*w+x)*2]<<8;
  bool border=x<insets.left||x>=w-insets.right||y<insets.top||y>=h-insets.bottom;
  if(border)assert(p==prof.background_rgb565);else if(p!=prof.background_rgb565)++ink;
 }assert(ink&&submits);
 touch_tap(0,0);risc_scene_event_v1 event;assert(tick(&event)==RISC_SCENE_IDLE);depth(1);
 unsigned lefts[]={insets.left,insets.top,insets.right,insets.bottom};unsigned tops[]={insets.top,insets.right,insets.bottom,insets.left};unsigned rights[]={insets.right,insets.bottom,insets.left,insets.top};
 unsigned q=rotation/90,fw=q%2?h:w;unsigned x=lefts[q]+(fw-lefts[q]-rights[q])/2,y=tops[q]+prof.font_scale*7+prof.padding*2+8+prof.row_height+10;
 unsigned px=x,py=y;if(q==1){px=w-1-y;py=x;}else if(q==2){px=w-1-x;py=h-1-y;}else if(q==3){px=y;py=h-1-x;}
 touch_tap(px,py);assert(tick(&event)==RISC_SCENE_IDLE);depth(2);settle();finish();printf("insets %ux%u rotation%u %s PASS\n",w,h,rotation,argv[4]);
}

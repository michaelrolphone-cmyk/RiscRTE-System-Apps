#define main existing_scene_main
#include "../scene/host_test.c"
#undef main
#include "RiscSceneKeypadV1.h"
static risc_components_document_v1 keys_doc;
static risc_display_insets_v1 insets;
static bool inset_info(void*c,risc_display_info_v1*out){bool ok=get_info(c,out);out->safe_insets=insets;return ok;}
int main(int argc,char**argv){
 assert(argc==2);unsigned shape=(unsigned)atoi(argv[1]);
 if(shape==0){w=h=240;format=5;}else if(shape==1){w=h=466;format=5;}else if(shape==2){w=480;h=800;format=1;}else if(shape==3){w=320;h=240;format=5;}else if(shape==4){w=h=466;format=5;insets=(risc_display_insets_v1){69,69,69,69};}else{w=320;h=300;format=5;insets=(risc_display_insets_v1){29,17,37,13};}
 prof.preferred_format=format;prof.font_scale=1;prof.row_height=36;
 expected_intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;
 driver=t5_driver_get(2);api=driver->capability;
 const risc_scene_components_api_v1 *ca=risc_scene_components_get_v1(api);assert(ca);
 risc_display_output_api_v1 display=disp;display.get_info=inset_info;
 risc_provider_dependency_v1 deps[]={{"display.output",1,&display},{"input.touch.raw",1,&raw},{"input.navigation",1,&nav},{"platform.clock",1,&clk},{"ui.presentation-profile",1,&prof}};
 assert(driver->start(deps,5));
 keys_doc=(risc_components_document_v1){.api_version=1,.struct_size=sizeof(keys_doc),.revision=1,.root=1,.route_count=1,.node_count=21,.screen_key=1};keys_doc.routes[0]=(risc_scene_route_v1){.id=1,.title="CALCULATOR"};
 keys_doc.nodes[0]=(risc_scene_node_v1){.id=1,.route=1,.kind=RISC_SCENE_TEXT_NODE,.label="123.456",.text="6 DP"};
 const char* labels[]={"C","+/-","DEL","/","7","8","9","x","4","5","6","-","1","2","3","+","0",".","="};
 for(unsigned i=0;i<19;i++){keys_doc.nodes[i+1]=(risc_scene_node_v1){.id=i+2,.route=1,.kind=RISC_COMPONENT_KEY,.action=i+1};snprintf(keys_doc.nodes[i+1].label,RISC_SCENE_LABEL,"%s",labels[i]);}
 keys_doc.nodes[20]=(risc_scene_node_v1){.id=21,.route=1,.kind=RISC_COMPONENT_HEADER_ACTION,.action=100,.label="BACK"};keys_doc.details[20].symbol=RISC_SYMBOL_BACK;
 /* Invalid keypad semantics must be refused without opening a session. */
 risc_components_document_v1 invalid=keys_doc;
 invalid.nodes[1].action=0;assert(ca->open(NULL,&invalid,NULL,&session)==RISC_SCENE_INVALID);
 invalid=keys_doc;invalid.nodes[1].target=1;assert(ca->open(NULL,&invalid,NULL,&session)==RISC_SCENE_INVALID);
 invalid=keys_doc;snprintf(invalid.nodes[1].label,RISC_SCENE_LABEL,"TOO LONG");assert(ca->open(NULL,&invalid,NULL,&session)==RISC_SCENE_INVALID);
 invalid=keys_doc;invalid.details[1].secondary_action=7;assert(ca->open(NULL,&invalid,NULL,&session)==RISC_SCENE_INVALID);
 assert(ca->open(NULL,&keys_doc,NULL,&session)==0);settle();
 unsigned cw=w-insets.left-insets.right,ch=h-insets.top-insets.bottom;
 unsigned scale=cw>=400?2:1,top=84*scale,cell=(ch-top-8*scale)/5;if(cell<24*scale)cell=24*scale;
 for(unsigned key=0;key<19;key++){
   unsigned usable=cw-32*scale,col=key%4,row=key/4;
   touch_tap(insets.left+16*scale+(col*usable/4+(col+1)*usable/4)/2,insets.top+top+row*cell+cell/2);
   risc_scene_event_v1 e;bool found=false;
   for(unsigned j=0;j<1000;j++){int rc=tick(&e);assert(rc==0||rc==1);if(rc==0){assert(e.kind==RISC_SCENE_ACTION_EVENT&&e.action==key+1&&e.node==key+2);found=true;break;}}
   assert(found);settle();
 }
 const char* out=getenv("KEYPAD_PPM");if(out){FILE*f=fopen(out,"wb");assert(f);fprintf(f,"P6\n%u %u\n255\n",w,h);for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){unsigned char rgb[3];if(format==5){unsigned v=((unsigned short*)(pixels+16))[y*w+x];rgb[0]=(v>>11)*255/31;rgb[1]=((v>>5)&63)*255/63;rgb[2]=(v&31)*255/31;}else{unsigned v=(pixels[16+y*((w+7)/8)+x/8]>>(7-x%8))&1;rgb[0]=rgb[1]=rgb[2]=v?255:0;}fwrite(rgb,1,3,f);}fclose(f);}
 /* Generic keypad routes put route navigation in the header, not over keys. */
 keys_doc.revision++;keys_doc.node_count=20;keys_doc.routes[0].back_action=101;
 assert(ca->update(NULL,session,&keys_doc)==0);settle();
 touch_tap(insets.left+16*scale+((2* (cw-32*scale)/4)+(3*(cw-32*scale)/4))/2,insets.top+top+4*cell+cell/2);
 risc_scene_event_v1 e;bool found=false;for(unsigned j=0;j<1000;j++){int rc=tick(&e);if(rc==0){assert(e.action==19&&e.node==20);found=true;break;}}assert(found);settle();
 touch_tap(insets.left+30*scale,insets.top+22*scale);found=false;
 for(unsigned j=0;j<1000;j++){int rc=tick(&e);if(rc==0){assert(e.action==101);found=true;break;}}assert(found);
 finish();printf("keypad shape %u all19keys passed\n",shape);return 0;
}

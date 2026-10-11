/* Forward declarations for generic telemetry component layout and rendering. */
static bool tm_kind(unsigned k){return k>=RISC_COMPONENT_INSTRUMENT&&k<=RISC_COMPONENT_CODE_ROW;}
static int tm_layout(unsigned,int*,int*,int*,int*);
static bool tm_draw(unsigned,int,int,int,int);
static void tm_tabs(void);
static void tm_header(void);
static bool tm_tabbed(void){for(unsigned i=0;i<document.node_count;i++)if(document.nodes[i].route==current_route()&&document.nodes[i].kind==RISC_COMPONENT_TAB_BAR&&!(document.nodes[i].flags&RISC_SCENE_HIDDEN))return true;return false;}
static int tm_footer(void){
 int scale=width>=400?2:1;if(tm_tabbed())return 36*scale;
 if((document.flags&RISC_COMPONENTS_TELEMETRY)&&!(document.flags&(RISC_COMPONENTS_ROOT_HEADER|RISC_COMPONENTS_CONFIRM|RISC_COMPONENTS_ALERT))){
  int r=route_index(&document,current_route());if(r>=0&&document.routes[r].back_action)return 48*scale;
 }return 0;
}

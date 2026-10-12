/* Optional telemetry is absent. Validation rejects its flag, kinds and icons
 * before a document can reach rendering. Legacy component layout is unchanged. */
static bool tm_kind(unsigned k){return k>=RISC_COMPONENT_INSTRUMENT&&k<=RISC_COMPONENT_CODE_ROW;}
static int tm_layout(unsigned n,int*x,int*y,int*w,int*h){(void)n;*x=*y=*w=*h=0;return 0;}
static bool tm_draw(unsigned n,int x,int y,int w,int h){(void)n;(void)x;(void)y;(void)w;(void)h;return false;}
static void tm_tabs(void){}
static void tm_header(void){}
static bool tm_tabbed(void){return false;}
static int tm_footer(void){return 0;}
static void tm_icon(unsigned n,int x,int y,int s,uint16_t c){(void)n;(void)x;(void)y;(void)s;(void)c;}

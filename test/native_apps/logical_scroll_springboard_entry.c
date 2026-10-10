/* Observe admission before the real adapter enforces display ownership. */
#include "T5AppApi.h"
static const t5_app_api_v1 *logical_api(uint32_t version);
#define t5_app_get_api logical_api
#include "../../Apps/springboard.c"
#undef t5_app_get_api
extern void logical_board_request(unsigned index);
static const t5_app_api_v1 *original;
static t5_app_api_v1 mapped;
static bool logical_launch(uint32_t index){logical_board_request(index);return original->request_app_launch(index);}
static const t5_app_api_v1 *logical_api(uint32_t version){original=t5_app_get_api(version);mapped=*original;mapped.request_app_launch=logical_launch;return &mapped;}
bool native_system_test_open(void){return true;}

int logical_board_offset(void){
#ifdef PORTABLE_SPRINGBOARD_TOUCH_SCROLL
 return sbs_pages_offset(&sbs_scroll);
#else
 return (int)selected;
#endif
}

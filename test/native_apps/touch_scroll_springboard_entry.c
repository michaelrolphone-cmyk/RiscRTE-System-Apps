#include "T5AppApi.h"
#include <assert.h>
static const t5_app_api_v1 *fixture_api(uint32_t version);
#define t5_app_get_api fixture_api
#include "../../Apps/springboard.c"
#undef t5_app_get_api
extern unsigned fixture_count(void);
extern unsigned fixture_index(unsigned index);
static const t5_app_api_v1 *original;
static t5_app_api_v1 mapped;
static uint32_t mapped_count(void){return fixture_count();}
static bool mapped_get(uint32_t index,t5_app_manifest_t *out){return index<fixture_count()&&original->installed_apps_get(fixture_index(index),out);}
static bool mapped_launch(uint32_t index){return index<fixture_count()&&original->request_app_launch(fixture_index(index));}
static bool mapped_icon(int32_t x,int32_t y,const char *name,uint8_t size,bool black){
    bool result=original->draw_icon(x,y,name,size,black);
    if(!strcmp(name,"solid:f11b"))assert(result); /* No question-mark fallback. */
    return result;
}
static const t5_app_api_v1 *fixture_api(uint32_t version){original=t5_app_get_api(version);mapped=*original;mapped.draw_icon=mapped_icon;mapped.installed_apps_count=mapped_count;mapped.installed_apps_get=mapped_get;mapped.request_app_launch=mapped_launch;return &mapped;}
int fixture_offset(void){return sbs_pages_offset(&sbs_scroll);}
int fixture_render_offset(void){return sbs_submitted.q8/256;}
bool fixture_render_highlight(void){return sbs_submitted.highlight;}
int fixture_velocity(void){return sbs_scroll.velocity_q8;}
int fixture_limit(void){return sbs_scroll.limit;}
bool fixture_pressed(void){return sbs_pressed;}
bool fixture_pending(void){return sbs_pending;}
bool fixture_highlight_completed(const char *name){return sbs_completed.valid&&sbs_completed.highlight&&sbs_completed.selected<sbs_completed.count&&!strcmp(sbs_completed.names[sbs_completed.selected],name);}

void fixture_grid_geometry(void) {
    assert(SBS_PAGE_APPS==12);
    assert(count==fixture_count());
    assert(sbs_scroll.limit==(count?(int)((count-1)/12)*480:0));
    for(unsigned i=0;i<count;i++)assert(sbs_names[i][0]);
    for(unsigned i=0;i<40;i++) {
        int x,y;sbs_position(i,0,&x,&y);
        assert(x==80+(int)(i%3)*160+(int)(i/12)*480);
        assert(y==160+(int)((i%12)/3)*160);
    }
}
void fixture_time_text(char out[12],bool known,uint8_t hour,uint8_t minute){sbs_time_text(out,known,hour,minute);}

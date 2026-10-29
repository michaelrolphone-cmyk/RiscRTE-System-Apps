#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "RiscScenePageV1.h"
#include "RiscSceneTelemetryV1.h"
static int32_t open_components(void*c,const risc_components_document_v1*d,const risc_scene_navigation_v1*n,uint64_t*h){(void)c;(void)d;(void)n;(void)h;return RISC_SCENE_OK;}
static int32_t update_components(void*c,uint64_t h,const risc_components_document_v1*d){(void)c;(void)h;(void)d;return RISC_SCENE_OK;}
static int32_t geometry(void*c,risc_scene_page_geometry_v1*g){(void)c;(void)g;return RISC_SCENE_OK;}
static int32_t page(void*c,uint64_t h,const risc_scene_page_document_v1*d,const uint8_t*p,size_t n){(void)c;(void)h;(void)d;(void)p;(void)n;return RISC_SCENE_OK;}
int main(void){
 unsigned cases=0;
 assert(!risc_scene_components_get_v1(NULL));assert(!risc_scene_page_get_v1(NULL));++cases;
 risc_scene_api_v1 base={.api_version=1,.struct_size=sizeof(base)};
 assert(!risc_scene_components_get_v1(&base));assert(!risc_scene_page_get_v1(&base));++cases;
 risc_scene_page_api_v1 p;memset(&p,0,sizeof(p));p.components.lifecycle.base.api_version=1;p.components.lifecycle.base.struct_size=sizeof(p);
 assert(!risc_scene_components_get_v1(&p.components.lifecycle.base));++cases;
 p.components.tag=RISC_COMPONENTS_TAG;p.components.version=1;p.components.open=open_components;p.components.update=update_components;
 assert(risc_scene_components_get_v1(&p.components.lifecycle.base));assert(!risc_scene_page_get_v1(&p.components.lifecycle.base));++cases;
 p.page_tag=RISC_SCENE_PAGE_TAG;p.page_version=1;p.geometry=geometry;p.present_page=page;
 assert(risc_scene_page_get_v1(&p.components.lifecycle.base));++cases;
 p.components.lifecycle.base.struct_size=sizeof(risc_scene_components_api_v1);
 assert(risc_scene_components_get_v1(&p.components.lifecycle.base));assert(!risc_scene_page_get_v1(&p.components.lifecycle.base));++cases;
 p.components.lifecycle.base.struct_size=sizeof(p);p.page_version=2;assert(!risc_scene_page_get_v1(&p.components.lifecycle.base));++cases;
 p.page_version=1;p.present_page=NULL;assert(!risc_scene_page_get_v1(&p.components.lifecycle.base));++cases;
 p.present_page=page;p.components.lifecycle.base.api_version=2;assert(!risc_scene_components_get_v1(&p.components.lifecycle.base));assert(!risc_scene_page_get_v1(&p.components.lifecycle.base));++cases;
 p.components.lifecycle.base.api_version=1;p.components.tag=0;assert(!risc_scene_page_get_v1(&p.components.lifecycle.base));++cases;
 printf("%u optional-table admission cases passed; no provider/device calls\n",cases);return 0;
}

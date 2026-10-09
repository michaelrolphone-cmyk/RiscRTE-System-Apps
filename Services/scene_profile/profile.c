#include "SceneProfileV1.h"
#include "RiscProviderV2.h"
#include "RiscDisplayOutputV1.h"
#ifndef SCENE_PROFILE_PAPER
#define SCENE_PROFILE_PAPER 0
#endif
#ifndef SCENE_DISPLAY_ROTATION
#define SCENE_DISPLAY_ROTATION 0
#endif
#ifndef SCENE_TOUCH_ROTATION
#define SCENE_TOUCH_ROTATION 0
#endif
#ifndef SCENE_PROFILE_ID
#define SCENE_PROFILE_ID "scene-presentation-profile"
#endif
static const risc_scene_profile_v1 profile={1,sizeof(profile),
#if SCENE_PROFILE_PAPER
    RISC_DISPLAY_FORMAT_MONO1,3,88,20,0,0xffff,0,
#else
    RISC_DISPLAY_FORMAT_RGB565,2,42,8,0xffff,0x0841,0x05ff,
#endif
    SCENE_DISPLAY_ROTATION,SCENE_TOUCH_ROTATION,0};
static bool start(const risc_provider_dependency_v1 *deps,size_t count){(void)deps;return count==0;}
static void stop(void){}
static bool quiesce(void){return true;}
static const risc_driver_v2 driver={2,sizeof(driver),SCENE_PROFILE_ID,RISC_SCENE_PROFILE_CAPABILITY,1,&profile,start,stop,quiesce};
__attribute__((visibility("default"))) const risc_driver_v2 *t5_driver_get(uint32_t abi){return abi==2?&driver:0;}

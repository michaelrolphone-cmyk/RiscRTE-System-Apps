#pragma once
#include "RiscSceneComponentsV1.h"
typedef struct {risc_components_document_v1 document;risc_scene_navigation_v1 navigation;uint32_t flags;uint64_t session;} store_test_view;
typedef void(*store_test_observer)(store_test_view*);

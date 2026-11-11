#pragma once
#include <stdint.h>
typedef struct {uint32_t api_version,struct_size;void* context;int32_t (*connect)(void*,const char*,const char*);int32_t (*poll)(void*);int32_t (*close)(void*);int32_t (*access_point)(void*,const char*,const char*);} infra_station_v1;

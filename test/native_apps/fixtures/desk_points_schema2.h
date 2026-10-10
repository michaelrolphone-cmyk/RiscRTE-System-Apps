#pragma once
/* Reconstructed from the frozen Home 0.3.16 receipt and ELF wire layout.
 * Pure app-owned data: eight recurring points and two custom names/colors.
 * Load only in foreground; timer wake projects this copy without storage. */
#include "PointsRecords.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
enum { LEGACY_DESK_POINTS_UNAVAILABLE=0, LEGACY_DESK_POINTS_READY=1,
       LEGACY_DESK_POINTS_ERROR=2 };
#define LEGACY_DESK_POINTS_BYTES 90u
typedef struct { uint8_t status,bytes[LEGACY_DESK_POINTS_BYTES]; } legacy_desk_points_snapshot;
static inline bool legacy_desk_points_unpack(const legacy_desk_points_snapshot *s,
                                               points_config *config,points_meta *meta) {
    if(!s || !config || !meta || s->status>LEGACY_DESK_POINTS_ERROR)return false;
    *config=(points_config){0};*meta=(points_meta){0};
    if(s->status!=LEGACY_DESK_POINTS_READY) {
        for(unsigned i=0;i<sizeof(s->bytes);i++)if(s->bytes[i])return false;
        return true;
    }
    uint8_t record[POINTS_RECORD_SIZE]={0};memcpy(record,"PTC1",4);
    memcpy(record+4,s->bytes,56);alarm_write32(record+60,points_checksum(record));
    if(!points_config_decode(config,record,sizeof(record)))return false;
    meta->revision=alarm_read32(s->bytes+56);
    for(unsigned i=0;i<2;i++) {
        meta->custom[i].color=s->bytes[60+i*15];
        memcpy(meta->custom[i].name,s->bytes+61+i*15,14);
    }
    return points_meta_valid(meta);
}
static inline bool legacy_desk_points_valid(const legacy_desk_points_snapshot *s) {
    points_config config;points_meta meta;
    return legacy_desk_points_unpack(s,&config,&meta);
}
static inline bool legacy_desk_points_pack(legacy_desk_points_snapshot *out,
                                            const points_config *config,const points_meta *meta) {
    if(!out || !points_config_valid(config) || !points_meta_valid(meta))return false;
    legacy_desk_points_snapshot s={.status=LEGACY_DESK_POINTS_READY};
    uint8_t record[POINTS_RECORD_SIZE];points_config_encode(config,record);
    memcpy(s.bytes,record+4,56);alarm_write32(s.bytes+56,meta->revision);
    for(unsigned i=0;i<2;i++) {
        s.bytes[60+i*15]=meta->custom[i].color;
        memcpy(s.bytes+61+i*15,meta->custom[i].name,14);
    }
    *out=s;return true;
}

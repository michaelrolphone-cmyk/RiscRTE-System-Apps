#pragma once
#include <stdint.h>
/* Read-only resident applicability query. No capability probe or write.
 * OK means applicable now; BUSY means unavailable or unsettled. All request
 * flags/reserved and response flags are zero. FACE addresses watch_face's
 * canonical native-faces IDs, never the independent desk_clock_face setting. */
enum { RISC_CONTEXTS_FACE=0,RISC_CONTEXTS_POWER=1,RISC_CONTEXTS_SLEEP=2,
 RISC_CONTEXTS_DEEP=3,RISC_CONTEXTS_WIFI=4,RISC_CONTEXTS_BLUETOOTH=5,
 RISC_CONTEXTS_BRIGHTNESS=6,RISC_CONTEXTS_VOLUME=7,RISC_CONTEXTS_NOTIFY=8,
 RISC_CONTEXTS_DND=9,RISC_CONTEXTS_SETTING_COUNT=10 };
#define RISC_CONTEXTS_SETTING_QUERY(field) (UINT32_C(0x43530100)+(field))

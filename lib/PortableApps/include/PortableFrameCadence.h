#pragma once
#include <stdint.h>
#include "RiscDisplayOutputV1.h"
/* A scheduling target, never a throughput claim. Retaining/unknown formats keep
 * the caller's historical interval. Interactive RGB565 defaults to 50 Hz when
 * the provider does not advertise a nominal refresh, matching shared Home.
 * A useful advertised refresh can request up to 60 Hz; never slow an existing
 * caller or infer a fast panel from resolution/device identity. */
static inline uint32_t portable_frame_cadence_ms(uint32_t format,uint32_t flags,
                                                uint32_t nominal_millihz,
                                                uint32_t legacy_ms) {
    if(!legacy_ms)legacy_ms=40u;
    if(format!=RISC_DISPLAY_FORMAT_RGB565 || (flags&RISC_DISPLAY_INFO_RETAINS_IMAGE))return legacy_ms;
    uint32_t interval=20u;
    if(nominal_millihz) {
        /* Ceil avoids requesting faster than the advertised integer rate. */
        interval=1000000u/nominal_millihz+(1000000u%nominal_millihz!=0u);
        if(interval<17u)interval=17u;
    }
    return interval<legacy_ms?interval:legacy_ms;
}

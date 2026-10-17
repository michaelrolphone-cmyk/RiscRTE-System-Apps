#pragma once
#include "PortableQuickActions.h"
#include "RiscDisplayOutputV1.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Overlay on a fresh complete app image every frame. No retained pixels.
 * RGB565 little endian. Legacy callers retain 240x240; with the existing
 * PORTABLE_WATCH_NATIVE_HEIGHT or explicit PORTABLE_QUICK_NATIVE build flag,
 * pqa_set_dimensions opts into native
 * scanlines and shared native control geometry. Other apps omit native assets.
 * Tight and padded/unaligned surfaces work; active rows need stride>=width*2
 * and size>=((height-1)*stride+width*2), with overflow checks.
 * A valid closed controller is a no-op. Time is a bounded caller-owned label
 * (up to eight ASCII characters); NULL shows --:--. Battery 0 is valid.
 * Panels move in Q8 and have the source's 22px bottom fade; compositing uses
 * one scanline (480 bytes for legacy, at most 4096 native), never a second
 * full-frame buffer or a scaled final image. Native typography uses separately
 * rasterized 2x licensed masks and per-glyph antialiasing. */
bool pqa_render(risc_display_surface_v1 *surface, const pqa_state *state,
                const char *time_label, bool battery_valid, uint8_t battery_percent);
#ifdef PORTABLE_RASTER_SNAPSHOT
/* Private opt-in replay helper. Coordinates and validation still describe the
 * full surface; only the requested physical rows are modified. */
bool pqa_render_rows(risc_display_surface_v1 *surface, const pqa_state *state,
                     const char *time_label, bool battery_valid, uint8_t battery_percent,
                     unsigned first_row, unsigned row_count);
#endif
/* Same licensed masks for the capability-selected paper client. The callback
 * accepts logical pixels; no allocation or provider access occurs here. */
void pqa_paper_icon(unsigned index,int x,int y,int size,bool black,
                    void (*pixel)(int,int,uint32_t,unsigned));
#ifdef __cplusplus
}
#endif

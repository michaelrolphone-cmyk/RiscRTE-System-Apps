#pragma once
/* Shared adapter geometry is bounded by its actual ABI: touch snapshots use
 * uint16_t dimensions, signed drawing coordinates use int32_t, and display
 * surfaces carry uint32_t byte counts. No device-size or aspect-ratio policy. */
#include "RiscDisplayOutputV1.h"

static inline bool portable_display_dimensions(uint32_t width, uint32_t height) {
    return width && height && width <= UINT16_MAX && height <= UINT16_MAX;
}

static inline bool portable_display_geometry(uint32_t width, uint32_t height,
        uint32_t format, uint32_t *row_bytes, uint32_t *frame_bytes) {
    if (!portable_display_dimensions(width, height)) return false;
    uint32_t row;
    if (format == RISC_DISPLAY_FORMAT_MONO1) row = width / 8u + (width % 8u != 0u);
    else if (format == RISC_DISPLAY_FORMAT_RGB565) row = width * 2u;
    else return false;
    if (row > UINT32_MAX / height || row > SIZE_MAX / height) return false;
    if (row_bytes) *row_bytes = row;
    if (frame_bytes) *frame_bytes = row * height;
    return true;
}

static inline bool portable_display_surface_valid(const risc_display_surface_v1 *s,
        uint32_t width, uint32_t height, uint32_t format) {
    uint32_t row;
    return s && portable_display_geometry(width, height, format, &row, NULL) &&
        s->frame && s->pixels && s->width == width && s->height == height &&
        s->pixel_format == format && s->stride_bytes >= row &&
        s->stride_bytes <= UINT32_MAX / height &&
        s->stride_bytes <= SIZE_MAX / height &&
        s->size_bytes >= s->stride_bytes * height;
}

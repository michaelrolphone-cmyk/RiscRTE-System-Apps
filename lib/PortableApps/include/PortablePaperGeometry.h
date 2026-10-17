#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Project a half-open reference interval to the pixels accepted by the
 * existing inverse mapping floor(pixel * reference / extent). Use endpoints,
 * not separately rounded lengths. Some downsampled one-row intervals collapse;
 * neighbouring rows still cover the complete projected glyph/control span.
 * Offscreen intervals are clipped before conversion back to drawing ints. */
static inline bool portable_paper_project_axis(int32_t start, int32_t length,
        int32_t offset, uint32_t extent, uint32_t reference,
        int32_t *position, int32_t *size) {
    if (!position || !size || length <= 0 || !extent || extent > UINT16_MAX ||
        !reference) return false;
    /* Three int32_t terms times at most UINT16_MAX fit comfortably in int64_t. */
    int64_t first = ((int64_t)start + offset) * extent;
    int64_t last = ((int64_t)start + offset + length) * extent;
    /* C division already rounds negative values toward +infinity. */
    int64_t left = first / reference + (first > 0 && first % reference != 0);
    int64_t right = last / reference + (last > 0 && last % reference != 0);
    if (left < 0) left = 0;
    if (right > extent) right = extent;
    if (left >= right) return false;
    *position = (int32_t)left;
    *size = (int32_t)(right - left);
    return true;
}

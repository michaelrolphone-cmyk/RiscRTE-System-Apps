#pragma once
#include <stdbool.h>
#include <stdint.h>

/* value is already clipped to [0, reference], extent is uint16-bounded.
 * Keep ordinary paper coordinates on 32-bit division: freestanding Xtensa
 * apps do not import the compiler's 64-bit division/modulo helpers. */
static inline uint32_t portable_paper_ceil_endpoint(uint32_t value,
        uint32_t extent, uint32_t reference) {
    if (reference <= UINT16_MAX) {
        uint32_t product = value * extent;
        return product / reference + (product % reference != 0);
    }
    /* Rare wide-reference case: find the first pixel endpoint whose product
     * reaches value*extent. At most 16 comparisons, with no wide division. */
    uint64_t product = (uint64_t)value * extent;
    uint32_t low = 0, high = extent;
    while (low < high) {
        uint32_t middle = low + (high - low) / 2;
        if ((uint64_t)middle * reference < product) low = middle + 1;
        else high = middle;
    }
    return low;
}

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
    /* Clip promoted reference endpoints before multiplying. This is equivalent
     * to clipping projected signed-ceil endpoints to [0, extent]. */
    int64_t first = (int64_t)start + offset;
    int64_t last = first + length;
    if (last <= 0 || first >= reference) return false;
    if (first < 0) first = 0;
    if (last > reference) last = reference;
    uint32_t left = portable_paper_ceil_endpoint((uint32_t)first,extent,reference);
    uint32_t right = portable_paper_ceil_endpoint((uint32_t)last,extent,reference);
    if (left >= right) return false;
    *position = (int32_t)left;
    *size = (int32_t)(right - left);
    return true;
}

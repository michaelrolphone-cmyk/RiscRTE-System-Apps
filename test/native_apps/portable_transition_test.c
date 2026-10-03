#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "PortableTransition.h"

/* This oracle deliberately calls none of the production pt_* helpers. It
 * resums every clamped window, quantizes after each pass, and divides rather
 * than using the production sliding sums and shift-based interpolation. */
#define TEST_MAX_SIDE 320u
#define TEST_MAX_PIXELS (TEST_MAX_SIDE * TEST_MAX_SIDE)
#define GUARD_WORDS 8u
#define MAX_STRIDE_WORDS (TEST_MAX_SIDE + 7u)
#define FRAME_WORDS (MAX_STRIDE_WORDS * TEST_MAX_SIDE + 2u * GUARD_WORDS)
#define SCRATCH_WORDS (TEST_MAX_PIXELS + 2u * GUARD_WORDS)
#define SENTINEL 0xa55au

static uint16_t old_storage[FRAME_WORDS], old_snapshot[FRAME_WORDS];
static uint16_t fresh_storage[FRAME_WORDS], dst_storage[FRAME_WORDS];
static uint16_t expected_storage[FRAME_WORDS], scratch_storage[SCRATCH_WORDS];
static uint16_t reference_horizontal[TEST_MAX_PIXELS];
static uint16_t reference_old[TEST_MAX_PIXELS], reference_fresh[TEST_MAX_PIXELS];
static unsigned frame_count, rejection_count;

static uint16_t reference_color(const unsigned channels[3]) {
    return (uint16_t)(channels[0] * 2048u + channels[1] * 32u + channels[2]);
}

static unsigned reference_channel(uint16_t pixel, unsigned channel) {
    static const unsigned divisors[3] = {2048u, 32u, 1u};
    static const unsigned ranges[3] = {32u, 64u, 32u};
    return (pixel / divisors[channel]) % ranges[channel];
}

static uint16_t reference_mix(uint16_t old, uint16_t fresh, unsigned alpha) {
    unsigned channels[3];
    for (unsigned c = 0; c < 3; c++) {
        channels[c] = (reference_channel(old, c) * (256u - alpha) +
                       reference_channel(fresh, c) * alpha + 128u) / 256u;
    }
    return reference_color(channels);
}

static void reference_blur(const uint16_t *src, unsigned stride_words,
                           uint16_t *dst, unsigned width, unsigned height,
                           unsigned radius) {
    unsigned divisor = 2u * radius + 1u;
    for (unsigned y = 0; y < height; y++) {
        for (unsigned x = 0; x < width; x++) {
            unsigned channels[3] = {0, 0, 0};
            for (int offset = -(int)radius; offset <= (int)radius; offset++) {
                int sample_x = (int)x + offset;
                if (sample_x < 0) sample_x = 0;
                if (sample_x >= (int)width) sample_x = (int)width - 1;
                uint16_t pixel = src[(size_t)y * stride_words + (unsigned)sample_x];
                for (unsigned c = 0; c < 3; c++) {
                    channels[c] += reference_channel(pixel, c);
                }
            }
            for (unsigned c = 0; c < 3; c++) {
                channels[c] = (channels[c] + divisor / 2u) / divisor;
            }
            reference_horizontal[(size_t)y * width + x] = reference_color(channels);
        }
    }
    for (unsigned y = 0; y < height; y++) {
        for (unsigned x = 0; x < width; x++) {
            unsigned channels[3] = {0, 0, 0};
            for (int offset = -(int)radius; offset <= (int)radius; offset++) {
                int sample_y = (int)y + offset;
                if (sample_y < 0) sample_y = 0;
                if (sample_y >= (int)height) sample_y = (int)height - 1;
                uint16_t pixel = reference_horizontal[(size_t)sample_y * width + x];
                for (unsigned c = 0; c < 3; c++) {
                    channels[c] += reference_channel(pixel, c);
                }
            }
            for (unsigned c = 0; c < 3; c++) {
                channels[c] = (channels[c] + divisor / 2u) / divisor;
            }
            dst[(size_t)y * width + x] = reference_color(channels);
        }
    }
}

static void fill_words(uint16_t *buffer, size_t words, uint16_t value) {
    for (size_t i = 0; i < words; i++) buffer[i] = value;
}

static uint16_t pattern_pixel(unsigned x, unsigned y, unsigned width,
                              unsigned height, unsigned pattern, bool fresh,
                              uint32_t *random) {
    if (pattern == 1u) {
        bool corner = (x == 0u || x == width - 1u) &&
                      (y == 0u || y == height - 1u);
        return corner ? (fresh ? 0x07e0u : 0xf81fu) : (fresh ? 0x001fu : 0u);
    }
    if (pattern == 2u) {
        return ((x + y + (fresh ? 1u : 0u)) % 2u) ? 0xffffu : 0u;
    }
    if (pattern == 3u) return fresh ? 0xffffu : 0u;
    *random = *random * 1664525u + 1013904223u;
    return (uint16_t)(*random >> 8);
}

static void prepare_frame(unsigned width, unsigned height, unsigned old_padding,
                          unsigned dst_padding, unsigned pattern) {
    uint32_t random = 0x12345678u;
    fill_words(old_storage, FRAME_WORDS, SENTINEL);
    fill_words(fresh_storage, FRAME_WORDS, SENTINEL);
    for (unsigned y = 0; y < height; y++) {
        for (unsigned x = 0; x < width; x++) {
            old_storage[GUARD_WORDS + (size_t)y * (width + old_padding) + x] =
                pattern_pixel(x, y, width, height, pattern, false, &random);
            fresh_storage[GUARD_WORDS + (size_t)y * (width + dst_padding) + x] =
                pattern_pixel(x, y, width, height, pattern, true, &random);
        }
    }
    memcpy(old_snapshot, old_storage, sizeof(old_storage));
}

static void check_phase(unsigned width, unsigned height, unsigned old_padding,
                        unsigned dst_padding, unsigned alpha) {
    unsigned old_words = width + old_padding, dst_words = width + dst_padding;
    size_t pixels = (size_t)width * height;
    uint16_t *dst = dst_storage + GUARD_WORDS;
    uint16_t *expected = expected_storage + GUARD_WORDS;
    const uint16_t *old = old_storage + GUARD_WORDS;
    memcpy(dst_storage, fresh_storage, sizeof(dst_storage));
    memcpy(expected_storage, fresh_storage, sizeof(expected_storage));
    fill_words(scratch_storage, SCRATCH_WORDS, SENTINEL);

    if (alpha == 0u) {
        for (unsigned y = 0; y < height; y++) {
            for (unsigned x = 0; x < width; x++) {
                expected[(size_t)y * dst_words + x] = old[(size_t)y * old_words + x];
            }
        }
    } else if (alpha != 256u) {
        reference_blur(old, old_words, reference_old, width, height, (6u * alpha + 255u) / 256u);
        reference_blur(fresh_storage + GUARD_WORDS, dst_words, reference_fresh,
                       width, height, (6u * (256u - alpha) + 255u) / 256u);
        for (unsigned y = 0; y < height; y++) {
            for (unsigned x = 0; x < width; x++) {
                size_t i = (size_t)y * width + x;
                expected[(size_t)y * dst_words + x] =
                    reference_mix(reference_old[i], reference_fresh[i], alpha);
            }
        }
    }
    assert(portable_transition_rgb565(dst, dst_words * 2u, old, old_words * 2u,
                                     scratch_storage + GUARD_WORDS, pixels * 2u,
                                     width, height, alpha));
    if (memcmp(dst_storage, expected_storage, sizeof(dst_storage))) {
        fprintf(stderr, "Reference mismatch: %ux%u, strides %u/%u, alpha %u\n",
                width, height, old_words * 2u, dst_words * 2u, alpha);
        abort();
    }
    assert(!memcmp(old_storage, old_snapshot, sizeof(old_storage)));
    for (size_t i = 0; i < GUARD_WORDS; i++) {
        assert(scratch_storage[i] == SENTINEL);
    }
    for (size_t i = GUARD_WORDS + pixels; i < SCRATCH_WORDS; i++) {
        assert(scratch_storage[i] == SENTINEL);
    }
    if (alpha == 0u || alpha == 256u) {
        for (size_t i = GUARD_WORDS; i < GUARD_WORDS + pixels; i++) {
            assert(scratch_storage[i] == SENTINEL);
        }
    }
    frame_count++;
}

static void check_reference_frames(void) {
    static const unsigned shapes[][4] = {
        {1, 1, 0, 0}, {1, 13, 7, 3}, {17, 1, 3, 7}, {2, 3, 0, 0},
        {3, 2, 5, 1}, {5, 7, 0, 4}, {17, 13, 3, 7}, {17, 13, 0, 0},
        {1, 320, 3, 7}, {320, 1, 7, 3}
    };
    for (size_t s = 0; s < sizeof(shapes) / sizeof(shapes[0]); s++) {
        for (unsigned pattern = 0; pattern < 4; pattern++) {
            prepare_frame(shapes[s][0], shapes[s][1], shapes[s][2], shapes[s][3], pattern);
            for (unsigned alpha = 0; alpha <= 256u; alpha++) {
                check_phase(shapes[s][0], shapes[s][1], shapes[s][2], shapes[s][3], alpha);
            }
        }
    }
    /* Maximal two-dimensional frame, both endpoints, and every radius step. */
    static const unsigned phases[] = {
        0, 1, 42, 43, 85, 86, 127, 128, 129, 170, 171, 213, 214, 255, 256
    };
    for (unsigned pattern = 0; pattern < 3; pattern++) {
        prepare_frame(320, 320, 7, 3, pattern);
        for (size_t i = 0; i < sizeof(phases) / sizeof(phases[0]); i++) {
            check_phase(320, 320, 7, 3, phases[i]);
        }
    }
}

static uint16_t invalid_dst[128], invalid_old[128], invalid_scratch[128];
static uint16_t invalid_dst_copy[128], invalid_old_copy[128], invalid_scratch_copy[128];

static void expect_rejected(uint16_t *dst, uint32_t stride, const uint16_t *old,
                            uint32_t old_stride, uint16_t *scratch,
                            size_t scratch_bytes, unsigned width,
                            unsigned height, unsigned alpha) {
    fill_words(invalid_dst, 128, 0x1234u);
    fill_words(invalid_old, 128, 0x5678u);
    fill_words(invalid_scratch, 128, 0x9abcu);
    memcpy(invalid_dst_copy, invalid_dst, sizeof(invalid_dst));
    memcpy(invalid_old_copy, invalid_old, sizeof(invalid_old));
    memcpy(invalid_scratch_copy, invalid_scratch, sizeof(invalid_scratch));
    assert(!portable_transition_rgb565(dst, stride, old, old_stride, scratch,
                                      scratch_bytes, width, height, alpha));
    assert(!memcmp(invalid_dst, invalid_dst_copy, sizeof(invalid_dst)));
    assert(!memcmp(invalid_old, invalid_old_copy, sizeof(invalid_old)));
    assert(!memcmp(invalid_scratch, invalid_scratch_copy, sizeof(invalid_scratch)));
    rejection_count++;
}

static void check_invalid_arguments(void) {
    /* Repeat validation at endpoints to forbid bypassing safety checks. */
    static const unsigned phases[] = {0, 1, 128, 255, 256};
    for (size_t i = 0; i < sizeof(phases) / sizeof(phases[0]); i++) {
        unsigned a = phases[i];
#define REJECT(d, ds, o, os, s, bytes, w, h) \
        expect_rejected(d, ds, o, os, s, bytes, w, h, a)
        REJECT(NULL, 8, invalid_old, 8, invalid_scratch, 32, 4, 4);
        REJECT(invalid_dst, 8, NULL, 8, invalid_scratch, 32, 4, 4);
        REJECT(invalid_dst, 8, invalid_old, 8, NULL, 32, 4, 4);
        REJECT(invalid_dst, 8, invalid_old, 8, invalid_scratch, 32, 0, 4);
        REJECT(invalid_dst, 8, invalid_old, 8, invalid_scratch, 32, 4, 0);
        REJECT(invalid_dst, 642, invalid_old, 642, invalid_scratch, SIZE_MAX, 321, 1);
        REJECT(invalid_dst, 2, invalid_old, 2, invalid_scratch, SIZE_MAX, 1, 321);
        REJECT(invalid_dst, 8, invalid_old, 8, invalid_scratch, SIZE_MAX, UINT_MAX, UINT_MAX);
        REJECT(invalid_dst, 6, invalid_old, 8, invalid_scratch, 32, 4, 4);
        REJECT(invalid_dst, 8, invalid_old, 6, invalid_scratch, 32, 4, 4);
        REJECT(invalid_dst, 0, invalid_old, 8, invalid_scratch, 32, 4, 4);
        REJECT(invalid_dst, 8, invalid_old, 0, invalid_scratch, 32, 4, 4);
        REJECT(invalid_dst, 9, invalid_old, 8, invalid_scratch, 32, 4, 4);
        REJECT(invalid_dst, 8, invalid_old, 9, invalid_scratch, 32, 4, 4);
        REJECT(invalid_dst, 8, invalid_old, 8, invalid_scratch, 31, 4, 4);
        REJECT(invalid_dst, 8, invalid_old, 8, invalid_scratch, 0, 4, 4);
        REJECT((uint16_t *)((uint8_t *)invalid_dst + 1), 8,
               invalid_old, 8, invalid_scratch, 32, 4, 4);
        REJECT(invalid_dst, 8, (uint16_t *)((uint8_t *)invalid_old + 1),
               8, invalid_scratch, 32, 4, 4);
        REJECT(invalid_dst, 8, invalid_old, 8,
               (uint16_t *)((uint8_t *)invalid_scratch + 1), 32, 4, 4);

        /* Equal pointers, overlap in either address order, and padded rows. */
        REJECT(invalid_dst, 8, invalid_dst, 8, invalid_scratch, 32, 4, 4);
        REJECT(invalid_dst, 8, invalid_old, 8, invalid_dst, 32, 4, 4);
        REJECT(invalid_dst, 8, invalid_old, 8, invalid_old, 32, 4, 4);
        REJECT(invalid_dst, 8, invalid_dst + 1, 8, invalid_scratch, 32, 4, 4);
        REJECT(invalid_dst + 1, 8, invalid_dst, 8, invalid_scratch, 32, 4, 4);
        REJECT(invalid_dst, 8, invalid_old, 8, invalid_dst + 1, 32, 4, 4);
        REJECT(invalid_dst + 1, 8, invalid_old, 8, invalid_dst, 32, 4, 4);
        REJECT(invalid_dst, 8, invalid_old, 8, invalid_old + 1, 32, 4, 4);
        REJECT(invalid_dst, 8, invalid_old + 1, 8, invalid_old, 32, 4, 4);
        REJECT(invalid_dst, 32, invalid_dst + 16, 32, invalid_scratch, 16, 4, 2);
        REJECT(invalid_dst, 32, invalid_old, 32, invalid_dst + 16, 16, 4, 2);
        REJECT(invalid_dst, 32, invalid_old, 32, invalid_old + 16, 16, 4, 2);
        /* Even a scratch slice in the padding between rows is conservatively rejected. */
        REJECT(invalid_dst, 32, invalid_old, 32, invalid_dst + 4, 16, 4, 2);
        REJECT(invalid_dst, 32, invalid_old, 32, invalid_old + 4, 16, 4, 2);

        /* Synthetic wrapping addresses must be rejected before dereference. */
        uint16_t *near_end = (uint16_t *)(UINTPTR_MAX - (UINTPTR_MAX % 2u));
        REJECT(near_end, 8, invalid_old, 8, invalid_scratch, 32, 4, 4);
        REJECT(invalid_dst, 8, near_end, 8, invalid_scratch, 32, 4, 4);
        REJECT(invalid_dst, 8, invalid_old, 8, near_end, 32, 4, 4);
#if SIZE_MAX <= UINT32_MAX
        /* On 32-bit targets these spans overflow size_t before pointer math. */
        REJECT(invalid_dst, UINT32_MAX - 1u, invalid_old, 8, invalid_scratch, 32, 4, 4);
        REJECT(invalid_dst, 8, invalid_old, UINT32_MAX - 1u, invalid_scratch, 32, 4, 4);
#endif
#undef REJECT
    }
    expect_rejected(invalid_dst, 8, invalid_old, 8, invalid_scratch, 32, 4, 4, 257);
    expect_rejected(invalid_dst, 8, invalid_old, 8, invalid_scratch, 32, 4, 4, UINT_MAX);
}

static void check_adjacent_slices(void) {
    /* Ranges, not allocation identities: all six orderings must be accepted. */
    static const unsigned orders[6][3] = {
        {0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}
    };
    for (size_t p = 0; p < 6; p++) {
        for (unsigned alpha = 0; alpha <= 256u; alpha++) {
            uint16_t storage[3 * 12 + 2];
            fill_words(storage, 38, SENTINEL);
            uint16_t *dst = storage + 1 + orders[p][0] * 12;
            uint16_t *old = storage + 1 + orders[p][1] * 12;
            uint16_t *scratch = storage + 1 + orders[p][2] * 12;
            fill_words(dst, 12, 0x07e0u);
            fill_words(old, 12, 0xf81fu);
            assert(portable_transition_rgb565(dst, 8, old, 8, scratch, 24, 4, 3, alpha));
            for (unsigned i = 0; i < 12; i++) {
                assert(dst[i] == reference_mix(0xf81fu, 0x07e0u, alpha));
                assert(old[i] == 0xf81fu);
                if (alpha == 0u || alpha == 256u) assert(scratch[i] == SENTINEL);
            }
            assert(storage[0] == SENTINEL && storage[37] == SENTINEL);
            frame_count++;
        }
    }
    /* Last-row padding is not part of the image envelope. One-row images need
     * only their visible row, even if the supplied (unused) stride is huge. */
    uint16_t storage[3] = {0x07e0u, 0xf81fu, SENTINEL};
    assert(portable_transition_rgb565(storage, UINT32_MAX - 1u, storage + 1,
                                     UINT32_MAX - 1u, storage + 2, 2, 1, 1, 128));
    assert(storage[0] == reference_mix(0xf81fu, 0x07e0u, 128));
    assert(storage[1] == 0xf81fu);
    frame_count++;
}

static void check_exact_allocations(void) {
    /* ASan redzones immediately follow the minimal image/scratch extents;
     * sentinel-only tests cannot detect reads beyond the last visible row. */
    const unsigned width = 3, height = 2, dst_words = 8, old_words = 5;
    size_t dst_bytes = ((height - 1u) * dst_words + width) * 2u;
    size_t old_bytes = ((height - 1u) * old_words + width) * 2u;
    size_t scratch_bytes = width * height * 2u;
    uint16_t *dst = (uint16_t *)malloc(dst_bytes);
    uint16_t *old = (uint16_t *)malloc(old_bytes);
    uint16_t *scratch = (uint16_t *)malloc(scratch_bytes);
    assert(dst && old && scratch);
    for (unsigned alpha = 0; alpha <= 256u; alpha++) {
        fill_words(dst, dst_bytes / 2u, 0x07e0u);
        fill_words(old, old_bytes / 2u, 0xf81fu);
        fill_words(scratch, scratch_bytes / 2u, SENTINEL);
        assert(portable_transition_rgb565(dst, dst_words * 2u, old, old_words * 2u,
                                         scratch, scratch_bytes, width, height, alpha));
        for (unsigned y = 0; y < height; y++) {
            for (unsigned x = 0; x < width; x++) {
                assert(dst[y * dst_words + x] == reference_mix(0xf81fu, 0x07e0u, alpha));
            }
        }
        for (unsigned x = width; x < dst_words; x++) assert(dst[x] == 0x07e0u);
        frame_count++;
    }
    free(scratch);
    free(old);
    free(dst);
}

int main(void) {
    assert(PORTABLE_TRANSITION_MAX_SIDE == TEST_MAX_SIDE);
    assert(PORTABLE_TRANSITION_MAX_RADIUS == 6u);
    for(uint32_t ms=0;ms<PORTABLE_TRANSITION_DURATION_MS;ms++) {
        assert(portable_transition_alpha(ms)<256u);
        if(ms)assert(portable_transition_alpha(ms)>=portable_transition_alpha(ms-1u));
    }
    assert(portable_transition_alpha(0)==0u);
    assert(portable_transition_alpha(PORTABLE_TRANSITION_DURATION_MS/2)==128u);
    assert(portable_transition_alpha(PORTABLE_TRANSITION_DURATION_MS)==256u);
    assert(portable_transition_alpha(UINT32_MAX)==256u);
    check_reference_frames();
    check_invalid_arguments();
    check_adjacent_slices();
    check_exact_allocations();
    printf("RGB565 transition: %u frames, independent two-pass clamped reference, "
           "exact endpoints, strides, canaries and %u non-mutating rejections passed\n",
           frame_count, rejection_count);
    return 0;
}

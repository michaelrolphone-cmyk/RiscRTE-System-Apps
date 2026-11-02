#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define FRAME_WIDTH 800
#define FRAME_HEIGHT 480
#define FRAME_STRIDE 100
#define FRAME_BYTES 48000
#define FRAME_FORMAT "mono1-msb-black1"
/* Native UC8179 full-refresh representation. */
typedef struct { unsigned width, height, stride, bytes; const char *format, *sha256; } frame_spec;
bool frame_valid(const frame_spec *s);
bool frame_add(size_t *received, uint8_t *dst, const uint8_t *src, size_t length);
bool frame_path(const char *path, const char *artifact_id);
bool frame_can_render(int64_t now, int64_t deadline, bool healthy, bool complete);

/* Service control IDs are globally allocated: another participant creates gaps. */
bool frame_cursor_new(double sequence, int64_t cursor);

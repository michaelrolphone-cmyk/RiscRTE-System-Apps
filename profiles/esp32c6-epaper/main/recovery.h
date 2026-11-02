#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define RECOVERY_HEADER_BYTES 24u
#define RECOVERY_MAX_BYTES 50000u
void recovery_pack(uint8_t *out,const char *payload,size_t length,uint32_t sequence);
bool recovery_valid(const uint8_t *data,size_t size,uint32_t *sequence,size_t *length);
bool recovery_newer(uint32_t a,uint32_t b);

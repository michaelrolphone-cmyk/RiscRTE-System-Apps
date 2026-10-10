#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define RISC_ENTROPY_SOURCE_CAPABILITY "crypto.entropy"
#define RISC_ENTROPY_SOURCE_API_V1 1u
#define RISC_ENTROPY_SOURCE_BYTES_MAX 32u
enum {
 RISC_ENTROPY_SOURCE_OK=0, RISC_ENTROPY_SOURCE_INVALID=-1,
 RISC_ENTROPY_SOURCE_UNAVAILABLE=-2, RISC_ENTROPY_SOURCE_CONTEXT=-3,
 RISC_ENTROPY_SOURCE_RETAINED=-4, RISC_ENTROPY_SOURCE_BUSY=-5
};
/* Ordinary copied entropy service. Activation performs no generation. fill
 * borrows the caller's output for this call only, copies exactly 1..32 fresh
 * hardware-backed bytes on success and leaves output unchanged on failure.
 * UNAVAILABLE requires waiting for a valid source, never a fallback seed.
 * CONTEXT/RETAINED from a live operation require retaining the grant. There is
 * no radio control, persistence, key creation or credential store in this API. */
typedef struct {
 uint32_t api_version,struct_size;
 void *context;
 int32_t (*fill)(void*,void*,uint32_t);
} risc_entropy_source_v1;
#ifdef __cplusplus
}
#endif

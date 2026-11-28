#pragma once

#include <stddef.h>
#include <stdint.h>

namespace rear_ota_datagrams {
constexpr size_t maxRequest = 1081;
constexpr size_t maxReply = 63;

// Called serially under the recovery update lock.
bool active();
size_t handle(const uint8_t *packet, size_t size, const char *key, const char *bootId,
              uint8_t *reply, bool &keepAwake, bool &reboot);
}

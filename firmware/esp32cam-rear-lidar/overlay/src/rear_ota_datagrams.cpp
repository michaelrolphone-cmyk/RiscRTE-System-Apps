#include "rear_ota_datagrams.h"

#include <cstring>
#include <esp_heap_caps.h>
#include <esp_ota_ops.h>
#include <lwip/inet.h>
#include <mbedtls/md.h>
#include <mbedtls/sha256.h>

namespace rear_ota_datagrams {
namespace {
constexpr uint8_t magic[] = {'R', 'O', 'T', '2'};
constexpr size_t headerSize = 21;
constexpr size_t macSize = 32;
constexpr size_t blockSize = 1024;
constexpr uint32_t window = 32;
enum Operation : uint8_t { Begin = 1, Block = 2, Status = 3, Commit = 4, Reply = 128 };
enum Result : uint8_t { Ok = 0, Missing = 1, Busy = 2, Invalid = 3, FlashError = 4 };

struct Transfer {
    uint8_t id[16]{};
    uint8_t expectedHash[32]{};
    const esp_partition_t *partition = nullptr;
    esp_ota_handle_t flash = 0;
    uint8_t *blocks = nullptr;
    mbedtls_sha256_context hash;
    size_t size = 0;
    uint32_t base = 0;
    uint32_t bitmap = 0;
    bool open = false;
    bool committed = false;
};
Transfer transfer;

uint32_t getUint32(const uint8_t *data) {
    uint32_t value;
    memcpy(&value, data, sizeof(value));
    return ntohl(value);
}

void putUint32(uint8_t *data, uint32_t value) {
    value = htonl(value);
    memcpy(data, &value, sizeof(value));
}

bool authenticate(const char *key, const char *bootId, const uint8_t *packet, size_t size,
                  uint8_t *signature) {
    uint8_t signedData[32 + maxRequest - macSize];
    memcpy(signedData, bootId, 32);
    memcpy(signedData + 32, packet, size);
    return mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                          reinterpret_cast<const uint8_t *>(key), strlen(key),
                          signedData, size + 32, signature) == 0;
}

bool equalDigest(const uint8_t *a, const uint8_t *b) {
    uint8_t difference = 0;
    for (size_t i = 0; i < 32; ++i) difference |= a[i] ^ b[i];
    return difference == 0;
}

void reset(bool abortFlash) {
    if (abortFlash && transfer.open) esp_ota_abort(transfer.flash);
    transfer.open = false;
    heap_caps_free(transfer.blocks);
    transfer.blocks = nullptr;
    mbedtls_sha256_free(&transfer.hash);
    transfer.base = 0;
    transfer.bitmap = 0;
}

Result begin(const uint8_t *id, const uint8_t *body, size_t length) {
    if (length != 36) return Invalid;
    const size_t size = getUint32(body);
    const auto *partition = esp_ota_get_next_update_partition(nullptr);
    if (!partition || size < 32 || size > partition->size) return Invalid;
    if (transfer.open) {
        return transfer.size == size && equalDigest(transfer.expectedHash, body + 4) ? Ok : Invalid;
    }
    transfer.blocks = static_cast<uint8_t *>(heap_caps_malloc(window * blockSize, MALLOC_CAP_SPIRAM));
    if (!transfer.blocks) return FlashError;
    if (esp_ota_begin(partition, size, &transfer.flash) != ESP_OK) {
        heap_caps_free(transfer.blocks);
        transfer.blocks = nullptr;
        return FlashError;
    }
    transfer.open = true;
    transfer.partition = partition;
    transfer.size = size;
    memcpy(transfer.id, id, sizeof(transfer.id));
    memcpy(transfer.expectedHash, body + 4, sizeof(transfer.expectedHash));
    mbedtls_sha256_init(&transfer.hash);
    if (mbedtls_sha256_starts_ret(&transfer.hash, 0) == 0) return Ok;
    reset(true);
    return FlashError;
}

size_t blockLength(uint32_t index) {
    const size_t offset = index * blockSize;
    if (offset >= transfer.size) return 0;
    const size_t remaining = transfer.size - offset;
    return remaining < blockSize ? remaining : blockSize;
}

Result writeBlock(const uint8_t *body, size_t length) {
    if (length < 4) return Invalid;
    const uint32_t index = getUint32(body);
    const uint32_t limit = (transfer.size + blockSize - 1) / blockSize;
    if (index >= limit || length - 4 != blockLength(index)) return Invalid;
    if (index < transfer.base || index - transfer.base >= window) return Ok;

    memcpy(transfer.blocks + (index % window) * blockSize, body + 4, length - 4);
    transfer.bitmap |= UINT32_C(1) << (index - transfer.base);
    while (transfer.bitmap & 1) {
        uint8_t block[blockSize];
        const size_t count = blockLength(transfer.base);
        memcpy(block, transfer.blocks + (transfer.base % window) * blockSize, count);
        if (esp_ota_write(transfer.flash, block, count) != ESP_OK ||
            mbedtls_sha256_update_ret(&transfer.hash, block, count) != 0) {
            reset(true);
            return FlashError;
        }
        ++transfer.base;
        transfer.bitmap >>= 1;
    }
    return Ok;
}

Result commit(bool &reboot) {
    if (transfer.base * blockSize < transfer.size) return Missing;
    uint8_t digest[32];
    if (mbedtls_sha256_finish_ret(&transfer.hash, digest) != 0 ||
        !equalDigest(digest, transfer.expectedHash)) {
        reset(true);
        return Invalid;
    }
    const uint32_t blocks = transfer.base;
    const esp_err_t ended = esp_ota_end(transfer.flash);
    reset(false);
    if (ended != ESP_OK || esp_ota_set_boot_partition(transfer.partition) != ESP_OK)
        return FlashError;
    transfer.base = blocks;
    transfer.committed = true;
    reboot = true;
    return Ok;
}
}

bool active() { return transfer.open || transfer.committed; }

size_t handle(const uint8_t *packet, size_t size, const char *key, const char *bootId,
              uint8_t *reply, bool &keepAwake, bool &reboot) {
    if (size < headerSize + macSize || size > maxRequest || memcmp(packet, magic, sizeof(magic)))
        return 0;
    uint8_t expected[32];
    const size_t signedLength = size - macSize;
    if (!authenticate(key, bootId, packet, signedLength, expected) ||
        !equalDigest(expected, packet + signedLength))
        return 0;

    const uint8_t operation = packet[4];
    const uint8_t *id = packet + 5;
    const uint8_t *body = packet + headerSize;
    const size_t length = signedLength - headerSize;
    Result result;
    if (transfer.committed) {
        result = (operation == Commit && length == 0 && !memcmp(id, transfer.id, sizeof(transfer.id)))
            ? Ok : Busy;
    } else if (transfer.open && memcmp(id, transfer.id, sizeof(transfer.id))) {
        result = Busy;
    } else if (operation == Begin) {
        result = begin(id, body, length);
    } else if (!transfer.open) {
        result = Missing;
    } else if (operation == Block) {
        result = writeBlock(body, length);
        keepAwake = result == Ok;
        if (result == Ok) return 0;  // The sender requests a selective ACK after each burst.
    } else if (operation == Status && length == 0) {
        result = Ok;
    } else if (operation == Commit && length == 0) {
        result = commit(reboot);
    } else {
        result = Invalid;
    }
    keepAwake = result == Ok;
    memcpy(reply, packet, headerSize);
    reply[4] = Reply;
    reply[21] = operation;
    reply[22] = result;
    putUint32(reply + 23, transfer.base);
    putUint32(reply + 27, transfer.bitmap);
    if (!authenticate(key, bootId, reply, 31, reply + 31)) return 0;
    return maxReply;
}
}

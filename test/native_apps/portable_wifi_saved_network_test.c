/* Real shared credential codec plus fake tables. Never uses a radio/network. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../lib/PortableApps/include/PortableWifiCredentials.h"

static unsigned wiped;
static bool all_zero(const void *data, size_t size) {
    const uint8_t *p = data;
    for (size_t i = 0; i < size; ++i) if (p[i]) return false;
    return true;
}
/* Observe the consumer's real volatile-wipe call while its local still lives.
 * The codec was included above and is not mocked or substituted. */
static void observed_clear(portable_wifi_credentials *credentials) {
    portable_wifi_credentials_clear(credentials);
    assert(all_zero(credentials, sizeof(*credentials))); ++wiped;
}
#define portable_wifi_credentials_clear observed_clear
#include "../../lib/PortableApps/include/PortableWifiSavedNetwork.h"
#undef portable_wifi_credentials_clear

typedef struct { char key[16]; uint8_t data[64]; uint32_t size; bool present; } record;
static record records[5];
static unsigned count, reads, writes, fail_read, corrupt_read, status_calls, connect_calls;
static wifi_link_t link;
static bool accept_connect;
static portable_wifi_credentials copied;
static const portable_wifi_credentials expected = {"saved-network", "saved-passphrase"};
static record *find(const char *key, bool create) {
    for (unsigned i = 0; i < count; ++i) if (!strcmp(records[i].key, key)) return &records[i];
    if (!create) return NULL;
    assert(count < 5 && strlen(key) <= 15);
    record *r = &records[count++]; strcpy(r->key, key); return r;
}
static int32_t storage_get(void *context, const char *key, void *data, uint32_t capacity, uint32_t *size) {
    assert(context == records); ++reads; *size = 0;
    if (fail_read == reads) return RISC_KEY_VALUE_IO;
    record *r = find(key, false);
    if (!r || !r->present) return RISC_KEY_VALUE_NOT_FOUND;
    *size = r->size;
    if (capacity < r->size) return RISC_KEY_VALUE_BUFFER_SMALL;
    memcpy(data, r->data, r->size);
    if (corrupt_read == reads && r->size) ((uint8_t *)data)[0] ^= 0x80;
    return RISC_KEY_VALUE_OK;
}
static int32_t storage_put(void *context, const char *key, const void *data, uint32_t size) {
    assert(context == records && size <= 64); ++writes;
    record *r = find(key, true); memcpy(r->data, data, size); r->size = size; r->present = true;
    return RISC_KEY_VALUE_OK;
}
static const risc_key_value_v1 storage = {1, sizeof(storage), records, storage_get, storage_put};
static wifi_link_t wifi_status(void *context) { assert(context == &link); ++status_calls; return link; }
static bool wifi_connect(void *context, const char *ssid, const char *password) {
    assert(context == &link && link == WIFI_LINK_DOWN); ++connect_calls;
    assert(strlen(ssid) <= 32 && strlen(password) <= 63);
    memset(&copied, 0, sizeof(copied)); strcpy(copied.ssid, ssid); strcpy(copied.password, password);
    return accept_connect;
}
static int8_t forbidden_rssi(void *context) { (void)context; assert(!"consumer must not query RSSI"); return 0; }
static bool forbidden_addresses(void *context, wifi_ipv4_v1 *station, wifi_ipv4_v1 *ap) {
    (void)context; (void)station; (void)ap; assert(!"consumer must not query IP addresses"); return false;
}
static bool forbidden_operation(void *context) { (void)context; assert(!"consumer must not clean up or scan"); return false; }
static bool forbidden_scan_poll(void *context, garden_radio_scan_result_v1 *result) {
    (void)context; (void)result; assert(!"consumer must not scan"); return false;
}
static const wifi_api_v1 wifi = {
    .api_version = WIFI_API_V1, .struct_size = sizeof(wifi), .context = &link,
    .connect = wifi_connect, .status = wifi_status, .rssi = forbidden_rssi,
    .addresses = forbidden_addresses, .scan_start = forbidden_operation,
    .scan_poll = forbidden_scan_poll, .scan_cancel = forbidden_operation,
    .disconnect_checked = forbidden_operation
};
static void counters(void) {
    reads = writes = fail_read = corrupt_read = status_calls = connect_calls = wiped = 0;
    memset(&copied, 0, sizeof(copied)); link = WIFI_LINK_DOWN; accept_connect = true;
}
static void seed(const portable_wifi_credentials *value) {
    memset(records, 0, sizeof(records)); count = 0; counters();
    assert(portable_wifi_credentials_save(&storage, value) == PORTABLE_WIFI_CREDENTIALS_SAVED);
    counters();
}
static void call(const risc_key_value_v1 *kv, const wifi_api_v1 *w, int result, unsigned statuses, unsigned connections) {
    assert(portable_wifi_saved_network_connect(kv, w) == result);
    assert(wiped == 1 && !writes && status_calls == statuses && connect_calls == connections);
}
static void saved_profiles(void) {
    assert(PORTABLE_WIFI_SAVED_NETWORK_STORAGE_INSTANCE == 6 && PORTABLE_WIFI_SAVED_NETWORK_WIFI_INSTANCE == 15);
    assert(!strcmp(PORTABLE_WIFI_SAVED_NETWORK_WIFI_CAPABILITY, "net.wifi"));
    seed(&expected); call(&storage, &wifi, PORTABLE_WIFI_SAVED_NETWORK_STARTED, 1, 1);
    assert(!memcmp(&copied, &expected, sizeof(copied)) && link == WIFI_LINK_DOWN);
    counters(); accept_connect = false;
    call(&storage, &wifi, PORTABLE_WIFI_SAVED_NETWORK_CONNECT_FAILED, 1, 1);
    assert(!memcmp(&copied, &expected, sizeof(copied)));
    for (unsigned state = 1; state <= 2; ++state) {
        counters(); link = (wifi_link_t)state;
        call(&storage, &wifi, PORTABLE_WIFI_SAVED_NETWORK_BUSY, 1, 0);
        assert(link == (wifi_link_t)state && all_zero(&copied, sizeof(copied)));
    }
    counters(); link = (wifi_link_t)3;
    call(&storage, &wifi, PORTABLE_WIFI_SAVED_NETWORK_WIFI_UNAVAILABLE, 1, 0);
    counters(); link = (wifi_link_t)-1;
    call(&storage, &wifi, PORTABLE_WIFI_SAVED_NETWORK_WIFI_UNAVAILABLE, 1, 0);
    portable_wifi_credentials open = {"open-network", ""}; seed(&open);
    call(&storage, &wifi, PORTABLE_WIFI_SAVED_NETWORK_STARTED, 1, 1);
    assert(!memcmp(&open, &copied, sizeof(open)));
    portable_wifi_credentials max;
    memset(&max, 'z', sizeof(max)); max.ssid[32] = max.password[63] = 0; seed(&max);
    call(&storage, &wifi, PORTABLE_WIFI_SAVED_NETWORK_STARTED, 1, 1);
    assert(!memcmp(&max, &copied, sizeof(max)));
}
static void storage_errors(void) {
    memset(records, 0, sizeof(records)); count = 0; counters();
    call(&storage, &wifi, PORTABLE_WIFI_SAVED_NETWORK_EMPTY, 0, 0);
    counters(); call(&storage, NULL, PORTABLE_WIFI_SAVED_NETWORK_EMPTY, 0, 0);
    counters(); call(NULL, &wifi, PORTABLE_WIFI_SAVED_NETWORK_STORAGE_UNAVAILABLE, 0, 0);
    seed(&expected);
    assert(portable_wifi_credentials_forget(&storage) == PORTABLE_WIFI_CREDENTIALS_FORGOTTEN);
    counters(); call(&storage, &wifi, PORTABLE_WIFI_SAVED_NETWORK_EMPTY, 0, 0);
    seed(&expected); find(PORTABLE_WIFI_CREDENTIALS_SELECTOR_KEY, false)->data[0] = 0;
    call(&storage, &wifi, PORTABLE_WIFI_SAVED_NETWORK_INVALID, 0, 0);
    seed(&expected); find(portable_wifi_credentials_chunk_key(0, 1), false)->present = false;
    call(&storage, &wifi, PORTABLE_WIFI_SAVED_NETWORK_INVALID, 0, 0);
    for (unsigned at = 1; at <= 4; ++at) {
        seed(&expected); fail_read = at;
        call(&storage, &wifi, PORTABLE_WIFI_SAVED_NETWORK_STORAGE_UNAVAILABLE, 0, 0);
        seed(&expected); corrupt_read = at;
        call(&storage, &wifi, at == 4 ? PORTABLE_WIFI_SAVED_NETWORK_STORAGE_UNAVAILABLE :
            PORTABLE_WIFI_SAVED_NETWORK_INVALID, 0, 0);
    }
    for (unsigned fault = 0; fault < 4; ++fault) {
        seed(&expected); risc_key_value_v1 bad = storage;
        if (fault == 0) bad.api_version = 2;
        if (fault == 1) bad.struct_size = sizeof(bad) - 1;
        if (fault == 2) bad.get = NULL;
        if (fault == 3) bad.put = NULL;
        call(&bad, &wifi, PORTABLE_WIFI_SAVED_NETWORK_STORAGE_UNAVAILABLE, 0, 0);
    }
}
static void wifi_errors(void) {
    seed(&expected); call(&storage, NULL, PORTABLE_WIFI_SAVED_NETWORK_WIFI_UNAVAILABLE, 0, 0);
    for (unsigned fault = 0; fault < 10; ++fault) {
        counters(); wifi_api_v1 bad = wifi;
        if (fault == 0) bad.api_version = 2;
        if (fault == 1) bad.struct_size = WIFI_MANAGEMENT_V1_SIZE - 1;
        if (fault == 2) bad.connect = NULL;
        if (fault == 3) bad.status = NULL;
        if (fault == 4) bad.rssi = NULL;
        if (fault == 5) bad.addresses = NULL;
        if (fault == 6) bad.scan_start = NULL;
        if (fault == 7) bad.scan_poll = NULL;
        if (fault == 8) bad.scan_cancel = NULL;
        if (fault == 9) bad.disconnect_checked = NULL;
        assert(!portable_wifi_saved_network_api_valid(&bad));
        call(&storage, &bad, PORTABLE_WIFI_SAVED_NETWORK_WIFI_UNAVAILABLE, 0, 0);
    }
    /* The old prefix really ends before the suffix, not just a short size tag. */
    union { void *align; uint8_t bytes[WIFI_PREFIX_V1_SIZE]; } prefix;
    memcpy(prefix.bytes, &wifi, sizeof(prefix.bytes));
    uint32_t prefix_size = sizeof(prefix.bytes);
    memcpy(prefix.bytes + offsetof(wifi_api_v1, struct_size), &prefix_size, sizeof(prefix_size));
    counters(); call(&storage, (const wifi_api_v1 *)prefix.bytes, PORTABLE_WIFI_SAVED_NETWORK_WIFI_UNAVAILABLE, 0, 0);
    counters(); wifi_api_v1 exact = wifi; exact.struct_size = WIFI_MANAGEMENT_V1_SIZE;
    assert(portable_wifi_saved_network_api_valid(&exact));
    call(&storage, &exact, PORTABLE_WIFI_SAVED_NETWORK_STARTED, 1, 1);
    counters(); exact.struct_size += 64; /* Append-only later extensions remain valid. */
    call(&storage, &exact, PORTABLE_WIFI_SAVED_NETWORK_STARTED, 1, 1);
}
int main(void) {
    saved_profiles(); storage_errors(); wifi_errors();
    puts("portable saved Wi-Fi consumer: shared codec, grant contract, suffix validation, DOWN-only connect, no side effects, and wipe paths passed");
    return 0;
}

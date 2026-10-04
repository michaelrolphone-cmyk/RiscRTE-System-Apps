/* Fault-injection tests for the real header-only plaintext storage protocol.
 * No runtime, wireless, display, or filesystem provider is linked. */
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include "../../lib/PortableApps/include/PortableWifiCredentials.h"

typedef struct {
    char key[16];
    uint8_t data[65];
    uint32_t size;
    bool exists;
} blob;
typedef struct { blob blobs[8]; unsigned count; } store;
static store database;
static unsigned reads, writes, operations, crash_at, fault_write, fault_read;
static unsigned write_mode, read_mode;
static bool committed_live, committed_empty;
static jmp_buf crash;
enum { DROP_IO=1, PERSIST_IO, DROP_OK, CORRUPT_IO, CORRUPT_OK, DROP_CONTEXT };
enum { READ_IO=1, READ_MISMATCH, READ_MISSING, READ_CONTEXT };
static void boundary(void) {
    ++operations;
    if (crash_at == operations) longjmp(crash, 1);
}
static bool zero(const void *data, size_t size) {
    const uint8_t *p = data;
    for (size_t i = 0; i < size; ++i) if (p[i]) return false;
    return true;
}
static blob *lookup(const char *key, bool create) {
    size_t length = strlen(key);
    assert(length && length <= RISC_KEY_VALUE_KEY_MAX);
    for (size_t i = 0; i < length; ++i)
        assert((key[i] >= 'a' && key[i] <= 'z') || (key[i] >= '0' && key[i] <= '9') ||
            key[i] == '_' || key[i] == '.' || key[i] == '-');
    bool known = !strcmp(key, PORTABLE_WIFI_CREDENTIALS_SELECTOR_KEY);
    for (unsigned slot = 0; slot < 2; ++slot)
        for (unsigned part = 0; part < 2; ++part)
            known |= !strcmp(key, portable_wifi_credentials_chunk_key(slot, part));
    assert(known);
    for (unsigned i = 0; i < database.count; ++i)
        if (!strcmp(key, database.blobs[i].key)) return &database.blobs[i];
    if (!create) return NULL;
    assert(database.count < 8);
    blob *b = &database.blobs[database.count++];
    strcpy(b->key, key); return b;
}
static int32_t get(void *context, const char *key, void *data, uint32_t capacity, uint32_t *size) {
    assert(context == &database && data && (capacity == 32 || capacity == 64) && size);
    boundary(); ++reads; *size = 0;
    unsigned mode = reads == fault_read ? read_mode : 0;
    if (mode == READ_IO || mode == READ_CONTEXT) {
        boundary(); return mode == READ_IO ? RISC_KEY_VALUE_IO : RISC_KEY_VALUE_CONTEXT;
    }
    blob *b = lookup(key, false);
    if (!b || !b->exists || mode == READ_MISSING) { boundary(); return RISC_KEY_VALUE_NOT_FOUND; }
    *size = b->size;
    if (b->size > capacity) { boundary(); return RISC_KEY_VALUE_BUFFER_SMALL; }
    memcpy(data, b->data, b->size);
    if (mode == READ_MISMATCH && b->size) ((uint8_t *)data)[0] ^= 0x80;
    boundary(); return RISC_KEY_VALUE_OK;
}
static int32_t put(void *context, const char *key, const void *data, uint32_t size) {
    assert(context == &database && data && size && size <= RISC_KEY_VALUE_BLOB_MAX);
    boundary(); ++writes;
    unsigned mode = writes == fault_write ? write_mode : 0;
    if (mode == DROP_IO || mode == DROP_OK || mode == DROP_CONTEXT) {
        boundary();
        return mode == DROP_OK ? RISC_KEY_VALUE_OK : mode == DROP_CONTEXT ? RISC_KEY_VALUE_CONTEXT : RISC_KEY_VALUE_IO;
    }
    blob *b = lookup(key, true);
    memset(b->data, 0, sizeof(b->data)); memcpy(b->data, data, size); b->size = size; b->exists = true;
    if (mode == CORRUPT_IO || mode == CORRUPT_OK) b->data[0] ^= 0x80;
    if (!strcmp(key, PORTABLE_WIFI_CREDENTIALS_SELECTOR_KEY) && portable_wifi_credentials_selector_valid(b->data)) {
        committed_live = b->data[5] != 0; committed_empty = b->data[5] == 0;
    }
    boundary();
    return mode == PERSIST_IO || mode == CORRUPT_IO ? RISC_KEY_VALUE_IO : RISC_KEY_VALUE_OK;
}
static const risc_key_value_v1 api = {1, sizeof(api), &database, get, put};
static const portable_wifi_credentials old_value = {"existing-network", "old-passphrase"};
static const portable_wifi_credentials new_value = {"replacement-network", "new-passphrase"};
static void reset_faults(void) {
    reads = writes = operations = crash_at = fault_write = fault_read = write_mode = read_mode = 0;
    committed_live = committed_empty = false;
}
static void fresh(void) { memset(&database, 0, sizeof(database)); reset_faults(); }
static void expect(int expected, const portable_wifi_credentials *value) {
    portable_wifi_credentials loaded;
    memset(&loaded, 0xa5, sizeof(loaded));
    assert(portable_wifi_credentials_load(&api, &loaded) == expected);
    if (expected == PORTABLE_WIFI_CREDENTIALS_LOADED) {
        assert(value && !strcmp(value->ssid, loaded.ssid) && !strcmp(value->password, loaded.password));
        assert(zero(loaded.ssid + strlen(loaded.ssid), sizeof(loaded.ssid) - strlen(loaded.ssid)));
        assert(zero(loaded.password + strlen(loaded.password), sizeof(loaded.password) - strlen(loaded.password)));
    } else assert(zero(&loaded, sizeof(loaded)));
    portable_wifi_credentials_clear(&loaded);
}
static void seed(void) {
    fresh();
    assert(portable_wifi_credentials_save(&api, &new_value) == PORTABLE_WIFI_CREDENTIALS_SAVED);
    assert(portable_wifi_credentials_save(&api, &old_value) == PORTABLE_WIFI_CREDENTIALS_SAVED);
    assert(database.count == PORTABLE_WIFI_CREDENTIALS_KEY_COUNT && database.count <= 8);
    reset_faults();
}
static blob *selector(void) { return lookup(PORTABLE_WIFI_CREDENTIALS_SELECTOR_KEY, false); }
static void selector_crc(void) {
    portable_wifi_credentials_put_u32(selector()->data + 28,
        portable_wifi_credentials_crc(selector()->data, 28));
}
static void repair_record_crcs(void) {
    uint8_t chunks[128];
    unsigned slot = selector()->data[6];
    for (unsigned part = 0; part < 2; ++part) {
        blob *b = lookup(portable_wifi_credentials_chunk_key(slot, part), false);
        portable_wifi_credentials_put_u32(b->data + 60, portable_wifi_credentials_crc(b->data, 60));
        memcpy(chunks + 64 * part, b->data, 64);
    }
    portable_wifi_credentials_put_u32(selector()->data + 20, portable_wifi_credentials_crc(chunks, 128));
    selector_crc(); portable_wifi_credentials_wipe(chunks, sizeof(chunks));
}
static void assert_slots_zero(void) {
    for (unsigned slot = 0; slot < 2; ++slot)
        for (unsigned part = 0; part < 2; ++part) {
            blob *b = lookup(portable_wifi_credentials_chunk_key(slot, part), false);
            assert(b && b->exists && b->size == 64 && zero(b->data, 64));
        }
}
static void validation(void) {
    fresh();
    assert(PORTABLE_WIFI_CREDENTIALS_STORE_INSTANCE == 6);
    portable_wifi_credentials v = old_value, out;
    assert(portable_wifi_credentials_validate(&v));
    assert(!portable_wifi_credentials_validate(NULL));
    assert(portable_wifi_credentials_save(&api, NULL) == PORTABLE_WIFI_CREDENTIALS_INVALID);
    assert(portable_wifi_credentials_load(&api, NULL) == PORTABLE_WIFI_CREDENTIALS_INVALID);
    assert(!reads && !writes);
    memset(&out, 0xa5, sizeof(out));
    assert(portable_wifi_credentials_load(NULL, &out) == PORTABLE_WIFI_CREDENTIALS_UNAVAILABLE);
    assert(zero(&out, sizeof(out)));
    assert(portable_wifi_credentials_save(NULL, &v) == PORTABLE_WIFI_CREDENTIALS_UNAVAILABLE);
    assert(portable_wifi_credentials_forget(NULL) == PORTABLE_WIFI_CREDENTIALS_UNAVAILABLE);
    for (unsigned fault = 0; fault < 4; ++fault) {
        risc_key_value_v1 bad = api;
        if (fault == 0) bad.api_version = 2;
        if (fault == 1) bad.struct_size = sizeof(api) - 1;
        if (fault == 2) bad.get = NULL;
        if (fault == 3) bad.put = NULL;
        assert(!portable_wifi_credentials_api_valid(&bad));
        memset(&out, 0xa5, sizeof(out));
        assert(portable_wifi_credentials_load(&bad, &out) == PORTABLE_WIFI_CREDENTIALS_UNAVAILABLE && zero(&out, sizeof(out)));
        assert(portable_wifi_credentials_save(&bad, &v) == PORTABLE_WIFI_CREDENTIALS_UNAVAILABLE);
        assert(portable_wifi_credentials_forget(&bad) == PORTABLE_WIFI_CREDENTIALS_UNAVAILABLE);
    }
    for (unsigned fault = 0; fault < 6; ++fault) {
        v = old_value;
        if (fault == 0) v.ssid[0] = 0;
        if (fault == 1) memset(v.ssid, 'x', sizeof(v.ssid));
        if (fault == 2) memset(v.password, 'x', sizeof(v.password));
        if (fault == 3) strcpy(v.password, "1234567");
        if (fault == 4) v.password[3] = 31;
        if (fault == 5) v.password[3] = 127;
        portable_wifi_credentials before = v;
        assert(!portable_wifi_credentials_validate(&v));
        assert(portable_wifi_credentials_save(&api, &v) == PORTABLE_WIFI_CREDENTIALS_INVALID);
        assert(!memcmp(&v, &before, sizeof(v)) && !reads && !writes);
    }
    v = old_value; memset(v.password, 0, sizeof(v.password));
    assert(portable_wifi_credentials_save(&api, &v) == PORTABLE_WIFI_CREDENTIALS_SAVED); expect(0, &v);
    strcpy(v.password, "        "); assert(portable_wifi_credentials_validate(&v));
    v.ssid[0] = (char)0xc3; v.ssid[1] = (char)0xa9;
    assert(portable_wifi_credentials_save(&api, &v) == PORTABLE_WIFI_CREDENTIALS_SAVED); expect(0, &v);
    memset(&v, 'x', sizeof(v)); v.ssid[32] = 0; v.password[63] = 0;
    for (unsigned i = 0; i < 20; ++i) {
        v.ssid[0] = (char)('A' + i);
        assert(portable_wifi_credentials_save(&api, &v) == PORTABLE_WIFI_CREDENTIALS_SAVED); expect(0, &v);
    }
    assert(database.count == 5);
    memset(&v, 0x7f, sizeof(v)); strcpy(v.ssid, "a"); strcpy(v.password, "12345678");
    assert(portable_wifi_credentials_save(&api, &v) == PORTABLE_WIFI_CREDENTIALS_SAVED); expect(0, &v);
    portable_wifi_credentials_clear(&v); assert(zero(&v, sizeof(v)));
    portable_wifi_credentials_clear(NULL);
}
static void malformed(void) {
    seed(); store good = database;
    for (unsigned i = 0; i < 32; ++i) {
        database = good; selector()->data[i] ^= 0x80; expect(PORTABLE_WIFI_CREDENTIALS_INVALID, NULL);
    }
    for (unsigned size = 0; size <= 65; ++size) if (size != 32) {
        database = good; selector()->size = size; expect(PORTABLE_WIFI_CREDENTIALS_INVALID, NULL);
    }
    for (unsigned part = 0; part < 2; ++part) {
        for (unsigned i = 0; i < 64; ++i) {
            database = good;
            lookup(portable_wifi_credentials_chunk_key(selector()->data[6], part), false)->data[i] ^= 0x80;
            expect(PORTABLE_WIFI_CREDENTIALS_INVALID, NULL);
        }
        for (unsigned size = 0; size <= 65; ++size) if (size != 64) {
            database = good;
            lookup(portable_wifi_credentials_chunk_key(selector()->data[6], part), false)->size = size;
            expect(PORTABLE_WIFI_CREDENTIALS_INVALID, NULL);
        }
        database = good;
        lookup(portable_wifi_credentials_chunk_key(selector()->data[6], part), false)->exists = false;
        expect(PORTABLE_WIFI_CREDENTIALS_INVALID, NULL); /* No fallback to the older complete slot. */
    }
    /* Well-checksummed but semantically impossible selector records. */
    const unsigned fields[] = {4,5,6,7,16,16,17,17,18,19,24,25,26,27};
    const uint8_t values[] = {2,2,2,1,0,33,7,64,1,1,1,1,1,1};
    for (unsigned i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i) {
        database = good; selector()->data[fields[i]] = values[i]; selector_crc();
        expect(PORTABLE_WIFI_CREDENTIALS_INVALID, NULL);
    }
    database = good; memset(selector()->data + 8, 0, 8); selector_crc(); expect(PORTABLE_WIFI_CREDENTIALS_INVALID, NULL);
    /* Checksums alone cannot admit embedded NUL, invalid password, or nonzero padding. */
    const unsigned offsets[] = {0,31,32,47,48,94,95};
    for (unsigned i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
        database = good;
        unsigned offset = offsets[i], part = offset / 48;
        blob *b = lookup(portable_wifi_credentials_chunk_key(selector()->data[6], part), false);
        b->data[12 + offset % 48] = offset == 0 ? 0 : 127;
        repair_record_crcs(); expect(PORTABLE_WIFI_CREDENTIALS_INVALID, NULL);
    }
    for (unsigned field = 0; field < 12; ++field) {
        database = good;
        blob *b = lookup(portable_wifi_credentials_chunk_key(selector()->data[6], 0), false);
        b->data[field] ^= 0x40; repair_record_crcs(); expect(PORTABLE_WIFI_CREDENTIALS_INVALID, NULL);
    }
    database = good; selector()->exists = false; expect(PORTABLE_WIFI_CREDENTIALS_EMPTY, NULL);
    assert(portable_wifi_credentials_save(&api, &new_value) == PORTABLE_WIFI_CREDENTIALS_SAVED); expect(0, &new_value);
    database = good; selector()->data[0] = 0;
    assert(portable_wifi_credentials_save(&api, &new_value) == PORTABLE_WIFI_CREDENTIALS_SAVED); expect(0, &new_value);
    /* Generation wrap is defined; the freshly verified inactive slot is authoritative. */
    database = good; memset(selector()->data + 8, 0xff, 8);
    for (unsigned part = 0; part < 2; ++part)
        memset(lookup(portable_wifi_credentials_chunk_key(selector()->data[6], part), false)->data + 4, 0xff, 8);
    repair_record_crcs(); expect(0, &old_value);
    assert(portable_wifi_credentials_save(&api, &new_value) == PORTABLE_WIFI_CREDENTIALS_SAVED);
    assert(portable_wifi_credentials_generation(selector()->data) == 1); expect(0, &new_value);
}
static void save_write_faults(void) {
    seed(); store original = database;
    for (unsigned position = 1; position <= 3; ++position)
        for (unsigned mode = DROP_IO; mode <= DROP_CONTEXT; ++mode) {
            database = original; reset_faults(); fault_write = position; write_mode = mode;
            int result = portable_wifi_credentials_save(&api, &new_value);
            assert(result == (mode == PERSIST_IO ? PORTABLE_WIFI_CREDENTIALS_SAVED : PORTABLE_WIFI_CREDENTIALS_UNCONFIRMED));
            reset_faults();
            if (mode == PERSIST_IO) expect(0, &new_value);
            else if (position == 3 && (mode == CORRUPT_IO || mode == CORRUPT_OK)) expect(PORTABLE_WIFI_CREDENTIALS_INVALID, NULL);
            else expect(0, &old_value);
            assert(portable_wifi_credentials_save(&api, &new_value) == PORTABLE_WIFI_CREDENTIALS_SAVED);
            expect(0, &new_value); assert(database.count == 5);
        }
}
static void save_read_faults(void) {
    seed(); store original = database;
    assert(portable_wifi_credentials_save(&api, &new_value) == PORTABLE_WIFI_CREDENTIALS_SAVED);
    unsigned total_reads = reads;
    assert(total_reads == 8);
    for (unsigned position = 1; position <= total_reads; ++position)
        for (unsigned mode = READ_IO; mode <= READ_CONTEXT; ++mode) {
            database = original; reset_faults(); fault_read = position; read_mode = mode;
            int result = portable_wifi_credentials_save(&api, &new_value);
            if (position == 1 && (mode == READ_IO || mode == READ_CONTEXT)) {
                assert(result == PORTABLE_WIFI_CREDENTIALS_UNAVAILABLE && !writes);
            } else if (position == 1) assert(result == PORTABLE_WIFI_CREDENTIALS_SAVED);
            else assert(result == PORTABLE_WIFI_CREDENTIALS_UNCONFIRMED);
            bool live = committed_live; reset_faults();
            expect(0, live ? &new_value : &old_value);
            assert(portable_wifi_credentials_save(&api, &new_value) == PORTABLE_WIFI_CREDENTIALS_SAVED); expect(0, &new_value);
        }
    for (unsigned position = 1; position <= 4; ++position)
        for (unsigned mode = READ_IO; mode <= READ_CONTEXT; ++mode) {
            database = original; reset_faults(); fault_read = position; read_mode = mode;
            int expected = position == 4 ? PORTABLE_WIFI_CREDENTIALS_UNCONFIRMED :
                (mode == READ_IO || mode == READ_CONTEXT) ? PORTABLE_WIFI_CREDENTIALS_UNAVAILABLE :
                position == 1 && mode == READ_MISSING ? PORTABLE_WIFI_CREDENTIALS_EMPTY : PORTABLE_WIFI_CREDENTIALS_INVALID;
            expect(expected, NULL);
        }
    reset_faults();
}
static void interrupted_saves(bool previously_saved) {
    if (previously_saved) seed(); else fresh();
    store original = database;
    assert(portable_wifi_credentials_save(&api, &new_value) == PORTABLE_WIFI_CREDENTIALS_SAVED);
    unsigned boundaries = operations;
    for (unsigned at = 1; at <= boundaries; ++at) {
        database = original; reset_faults(); crash_at = at;
        if (!setjmp(crash)) { (void)portable_wifi_credentials_save(&api, &new_value); assert(!"missed interruption"); }
        bool live = committed_live; reset_faults();
        if (live) expect(0, &new_value);
        else if (previously_saved) expect(0, &old_value);
        else expect(PORTABLE_WIFI_CREDENTIALS_EMPTY, NULL);
        assert(portable_wifi_credentials_save(&api, &new_value) == PORTABLE_WIFI_CREDENTIALS_SAVED);
        expect(0, &new_value); assert(database.count <= 5);
    }
}
static void forget_faults(void) {
    seed(); store original = database;
    for (unsigned position = 1; position <= 5; ++position)
        for (unsigned mode = DROP_IO; mode <= DROP_CONTEXT; ++mode) {
            database = original; reset_faults(); fault_write = position; write_mode = mode;
            int result = portable_wifi_credentials_forget(&api);
            if (mode == PERSIST_IO) assert(result == PORTABLE_WIFI_CREDENTIALS_FORGOTTEN);
            else assert(result == (position == 1 ? PORTABLE_WIFI_CREDENTIALS_UNCONFIRMED : PORTABLE_WIFI_CREDENTIALS_CLEANUP_REMANENCE));
            assert(writes == (position == 1 && mode != PERSIST_IO ? 1u : 5u));
            reset_faults();
            if (position == 1 && (mode == CORRUPT_IO || mode == CORRUPT_OK)) expect(PORTABLE_WIFI_CREDENTIALS_INVALID, NULL);
            else if (position == 1 && mode != PERSIST_IO) expect(0, &old_value);
            else expect(PORTABLE_WIFI_CREDENTIALS_EMPTY, NULL);
            assert(portable_wifi_credentials_forget(&api) == PORTABLE_WIFI_CREDENTIALS_FORGOTTEN);
            expect(PORTABLE_WIFI_CREDENTIALS_EMPTY, NULL); assert_slots_zero();
            assert(portable_wifi_credentials_save(&api, &new_value) == PORTABLE_WIFI_CREDENTIALS_SAVED); expect(0, &new_value);
        }
    database = original; reset_faults();
    assert(portable_wifi_credentials_forget(&api) == PORTABLE_WIFI_CREDENTIALS_FORGOTTEN);
    unsigned total_reads = reads; assert(total_reads == 7);
    for (unsigned position = 1; position <= total_reads; ++position)
        for (unsigned mode = READ_IO; mode <= READ_CONTEXT; ++mode) {
            database = original; reset_faults(); fault_read = position; read_mode = mode;
            int result = portable_wifi_credentials_forget(&api);
            assert(result == (position == 1 ? PORTABLE_WIFI_CREDENTIALS_FORGOTTEN :
                position == 2 || position == 7 ? PORTABLE_WIFI_CREDENTIALS_UNCONFIRMED : PORTABLE_WIFI_CREDENTIALS_CLEANUP_REMANENCE));
            assert(writes == (position == 2 ? 1u : 5u));
            reset_faults(); expect(PORTABLE_WIFI_CREDENTIALS_EMPTY, NULL);
            assert(portable_wifi_credentials_forget(&api) == PORTABLE_WIFI_CREDENTIALS_FORGOTTEN); assert_slots_zero();
        }
    /* A confirmed tombstone wins even over complete old slots restored later. */
    database = original; reset_faults();
    assert(portable_wifi_credentials_forget(&api) == PORTABLE_WIFI_CREDENTIALS_FORGOTTEN);
    blob tombstone = *selector(); database = original; *selector() = tombstone;
    expect(PORTABLE_WIFI_CREDENTIALS_EMPTY, NULL);
    for (unsigned i = 0; i < 4; ++i) {
        database = original; *selector() = tombstone;
        unsigned field = i == 0 ? 6 : i == 1 ? 16 : i == 2 ? 17 : 20;
        selector()->data[field] = 1; selector_crc(); expect(PORTABLE_WIFI_CREDENTIALS_INVALID, NULL);
    }
    database = original; selector()->data[0] = 0;
    assert(portable_wifi_credentials_forget(&api) == PORTABLE_WIFI_CREDENTIALS_FORGOTTEN); expect(PORTABLE_WIFI_CREDENTIALS_EMPTY, NULL);
    fresh(); assert(portable_wifi_credentials_forget(&api) == PORTABLE_WIFI_CREDENTIALS_FORGOTTEN);
    assert(database.count == 5); assert_slots_zero(); expect(PORTABLE_WIFI_CREDENTIALS_EMPTY, NULL);
}
static void interrupted_forget(void) {
    seed(); store original = database;
    assert(portable_wifi_credentials_forget(&api) == PORTABLE_WIFI_CREDENTIALS_FORGOTTEN);
    unsigned boundaries = operations;
    for (unsigned at = 1; at <= boundaries; ++at) {
        database = original; reset_faults(); crash_at = at;
        if (!setjmp(crash)) { (void)portable_wifi_credentials_forget(&api); assert(!"missed interruption"); }
        bool forgotten = committed_empty; reset_faults();
        if (forgotten) expect(PORTABLE_WIFI_CREDENTIALS_EMPTY, NULL); else expect(0, &old_value);
        assert(portable_wifi_credentials_forget(&api) == PORTABLE_WIFI_CREDENTIALS_FORGOTTEN);
        assert_slots_zero(); expect(PORTABLE_WIFI_CREDENTIALS_EMPTY, NULL);
    }
}
int main(void) {
    validation(); malformed(); save_write_faults(); save_read_faults();
    interrupted_saves(false); interrupted_saves(true); forget_faults(); interrupted_forget();
    puts("portable Wi-Fi credential storage: validation, corruption, interrupted/uncertain writes, retries, tombstones, and key budget passed");
    return 0;
}

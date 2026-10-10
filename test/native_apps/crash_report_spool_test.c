#include "CrashReportSpool.h"
#include <assert.h>
#include <stdio.h>

/* No real filesystem, SD, formatter or acknowledgement. Faults model the
 * documented AppData destination/revision contract, including restart. */
typedef struct {
    uint8_t bytes[RISC_APP_DATA_FILE_MAX];
    uint32_t size;
    uint64_t revision;
    bool present;
    unsigned stats, reads, writes;
    int32_t stat_fault, read_fault, write_fault;
    int32_t after_write_stat_fault, after_write_read_fault;
    bool commit_on_fault, corrupt_on_fault, collision_on_fault;
    bool bad_stat_revision, bad_stat_absence, bad_read_size, bad_read_revision;
    uint64_t last_expected;
} fake_data;
static fake_data backend;
static crash_report_spool spool, rebooted;
static uint8_t saved[RISC_APP_DATA_FILE_MAX];
static uint32_t saved_size;
static char maximum_text[CRASH_REPORT_SPOOL_TEXT_MAX + 1];
static unsigned tests;

static int32_t fake_stat(void *context, const char *name, uint32_t *size, uint64_t *revision) {
    fake_data *f = context;
    assert(!strcmp(name, "crash-spool.bin"));
    ++f->stats; *size = 0; *revision = 0;
    int32_t fault = f->stat_fault; f->stat_fault = 0;
    if (fault) return fault;
    if (!f->present) {
        if (f->bad_stat_absence) *revision = 99;
        return RISC_APP_DATA_NOT_FOUND;
    }
    *size = f->size; *revision = f->bad_stat_revision ? 0 : f->revision;
    return RISC_APP_DATA_OK;
}
static int32_t fake_read(void *context, const char *name, uint64_t expected, void *bytes,
                         uint32_t capacity, uint32_t *size, uint64_t *revision) {
    fake_data *f = context;
    assert(!strcmp(name, "crash-spool.bin"));
    ++f->reads; *size = 0; *revision = 0;
    int32_t fault = f->read_fault; f->read_fault = 0;
    if (fault) return fault;
    if (expected != (f->present ? f->revision : 0)) return RISC_APP_DATA_STALE;
    if (!f->present) return RISC_APP_DATA_NOT_FOUND;
    if (capacity < f->size) { *size = f->size; return RISC_APP_DATA_BUFFER_SMALL; }
    memcpy(bytes, f->bytes, f->size);
    *size = f->size + (f->bad_read_size ? 1u : 0u);
    *revision = f->revision + (f->bad_read_revision ? 1u : 0u);
    return RISC_APP_DATA_OK;
}
static void repair_crc(void) {
    assert(backend.size >= CRASH_REPORT_SPOOL_HEADER_SIZE);
    crash_report_spool_put32(backend.bytes + 76, crash_report_spool_crc(backend.bytes, backend.size));
}
static int32_t fake_replace(void *context, const char *name, uint64_t expected, const void *bytes, uint32_t size) {
    fake_data *f = context;
    assert(!strcmp(name, "crash-spool.bin"));
    assert(size <= CRASH_REPORT_SPOOL_FILE_MAX && size <= RISC_APP_DATA_FILE_MAX);
    ++f->writes; f->last_expected = expected;
    if (expected != (f->present ? f->revision : 0)) return RISC_APP_DATA_STALE;
    int32_t fault = f->write_fault; f->write_fault = 0;
    if (!fault || f->commit_on_fault) {
        /* STALE+commit models a competing writer installing the same bytes;
         * the rejected replace itself does not modify the destination. */
        memcpy(f->bytes, bytes, size); f->size = size; f->present = true;
    }
    if (f->corrupt_on_fault) {
        assert(f->present && f->size);
        f->bytes[f->size - 1] ^= 0x80u; /* torn/corrupted disk readback */
    }
    if (f->collision_on_fault) {
        assert(f->size > CRASH_REPORT_SPOOL_HEADER_SIZE + CRASH_REPORT_SPOOL_ENTRY_HEADER_SIZE);
        f->bytes[CRASH_REPORT_SPOOL_HEADER_SIZE + CRASH_REPORT_SPOOL_ENTRY_HEADER_SIZE] = 'Z';
        repair_crc();
    }
    if (!fault || fault == RISC_APP_DATA_STALE || fault == RISC_APP_DATA_COMMIT_UNKNOWN) ++f->revision;
    f->stat_fault = f->after_write_stat_fault; f->read_fault = f->after_write_read_fault;
    f->after_write_stat_fault = f->after_write_read_fault = 0;
    f->commit_on_fault = f->corrupt_on_fault = f->collision_on_fault = false;
    return fault;
}
static risc_app_data_v1 api = {
    RISC_APP_DATA_API_V1, sizeof(risc_app_data_v1), &backend, fake_stat, fake_read, fake_replace
};
static void reset(void) {
    memset(&backend, 0, sizeof(backend)); backend.revision = 17;
    assert(crash_report_spool_init(&spool, &api) == CRASH_REPORT_SPOOL_OK);
    assert(crash_report_spool_init(&rebooted, &api) == CRASH_REPORT_SPOOL_OK);
}
static crash_report_spool_identity identity(uint32_t n) {
    crash_report_spool_identity id;
    for (unsigned i = 0; i < 32; ++i) id.firmware_sha256[i] = (uint8_t)(i + n);
    id.captured_boot = UINT32_C(0x01020304) + n;
    id.captured_sequence = UINT32_C(0xa0b0c0d0) + n;
    return id;
}
static crash_report_spool_result append(uint32_t n, const char *text) {
    crash_report_spool_identity id = identity(n);
    return crash_report_spool_append(&spool, &id, text, (uint32_t)strlen(text));
}
static void assert_entry(uint32_t index, uint32_t n, const char *text) {
    crash_report_spool_view view;
    crash_report_spool_identity id = identity(n);
    assert(crash_report_spool_peek(&spool, index, &view) == CRASH_REPORT_SPOOL_OK);
    assert(!memcmp(view.identity.firmware_sha256, id.firmware_sha256, 32));
    assert(view.identity.captured_boot == id.captured_boot);
    assert(view.identity.captured_sequence == id.captured_sequence);
    assert(view.text_size == strlen(text) && !memcmp(view.text, text, view.text_size));
}
static void save_disk(void) { saved_size = backend.size; memcpy(saved, backend.bytes, saved_size); }
static void assert_unchanged(void) {
    assert(backend.size == saved_size && !memcmp(backend.bytes, saved, saved_size));
}
static void restore_disk(void) {
    backend.size = saved_size; memcpy(backend.bytes, saved, saved_size); backend.present = true;
}
static void assert_corrupt_no_write(void) {
    unsigned writes = backend.writes;
    assert(crash_report_spool_reload(&spool) == CRASH_REPORT_SPOOL_CORRUPT);
    assert(!spool.loaded);
    assert(append(999, "new report\n") == CRASH_REPORT_SPOOL_CORRUPT);
    assert(backend.writes == writes);
}
static void test_first_queue_encoding_restart(void) {
    reset();
    assert(crash_report_spool_reload(&spool) == CRASH_REPORT_SPOOL_OK);
    assert(spool.loaded && !spool.present && !spool.count && !spool.dropped_count);
    assert(!backend.reads && !backend.writes && !backend.present);
    assert(append(0, "a\nb\n") == CRASH_REPORT_SPOOL_COMMITTED);
    assert(backend.last_expected == 0 && backend.writes == 1);
    assert(backend.size == 132 && !memcmp(backend.bytes, "CRSHSPL\0", 8));
    assert(backend.bytes[8] == 1 && backend.bytes[9] == 0 && backend.bytes[10] == 80 && backend.bytes[11] == 0);
    assert(backend.bytes[12] == 132 && backend.bytes[13] == 0 && backend.bytes[16] == 1);
    for (unsigned i = 0; i < 32; ++i) assert(backend.bytes[80 + i] == i);
    assert(backend.bytes[112] == 4 && backend.bytes[113] == 3 && backend.bytes[114] == 2 && backend.bytes[115] == 1);
    assert(backend.bytes[116] == 0xd0 && backend.bytes[117] == 0xc0 && backend.bytes[118] == 0xb0 && backend.bytes[119] == 0xa0);
    assert(backend.bytes[120] == 4 && !memcmp(backend.bytes + 128, "a\nb\n", 4));
    /* Independent golden CRC generated with Python zlib.crc32. */
    assert(crash_report_spool_u32(backend.bytes + 76) == UINT32_C(0x76626f3c));
    assert_entry(0, 0, "a\nb\n"); save_disk();
    backend.revision = 8001; /* reboot/mount-session tokens must be reacquired */
    assert(crash_report_spool_reload(&rebooted) == CRASH_REPORT_SPOOL_OK);
    assert(rebooted.count == 1 && rebooted.revision == 8001);
    crash_report_spool_identity id = identity(0);
    assert(crash_report_spool_append(&rebooted, &id, "a\nb\n", 4) == CRASH_REPORT_SPOOL_COMMITTED);
    assert(backend.writes == 1); assert_unchanged();
    assert(append(1, "second\n") == CRASH_REPORT_SPOOL_COMMITTED);
    assert(backend.last_expected == 8001 && spool.count == 2);
    assert_entry(0, 0, "a\nb\n"); assert_entry(1, 1, "second\n");
    ++tests;
}
static void test_exact_dedupe_variants(void) {
    reset(); assert(append(1, "alpha\n") == CRASH_REPORT_SPOOL_COMMITTED);
    unsigned writes = backend.writes;
    assert(append(1, "alpha\n") == CRASH_REPORT_SPOOL_COMMITTED && backend.writes == writes);
    assert(append(1, "alphb\n") == CRASH_REPORT_SPOOL_COMMITTED && backend.writes == writes + 1);
    assert(append(1, "alpha") == CRASH_REPORT_SPOOL_COMMITTED && backend.writes == writes + 2);
    writes = backend.writes;
    assert(append(1, "alphb\n") == CRASH_REPORT_SPOOL_COMMITTED && backend.writes == writes);
    assert(append(1, "alpha") == CRASH_REPORT_SPOOL_COMMITTED && backend.writes == writes);
    crash_report_spool_identity id = identity(1);
    assert(crash_report_spool_remove(&spool, &id, "other\n", 6) == CRASH_REPORT_SPOOL_REMOVED);
    assert(backend.writes == writes && spool.count == 3);
    id.firmware_sha256[31] ^= 1; /* Full hash, not a display prefix. */
    assert(crash_report_spool_append(&spool, &id, "alpha\n", 6) == CRASH_REPORT_SPOOL_COMMITTED);
    id = identity(1); ++id.captured_boot;
    assert(crash_report_spool_append(&spool, &id, "alpha\n", 6) == CRASH_REPORT_SPOOL_COMMITTED);
    id = identity(1); ++id.captured_sequence;
    assert(crash_report_spool_append(&spool, &id, "alpha\n", 6) == CRASH_REPORT_SPOOL_COMMITTED);
    assert(spool.count == 6);
    crash_report_spool_view view;
    assert(crash_report_spool_peek(&spool, 0, &view) == CRASH_REPORT_SPOOL_OK);
    writes = backend.writes;
    assert(crash_report_spool_append(&spool, &view.identity, view.text, view.text_size) == CRASH_REPORT_SPOOL_COMMITTED);
    assert(backend.writes == writes); /* aliased peek input survives reload */
    ++tests;
}

static void test_reused_capture_and_recovery_reset_variants(void) {
    static const char first[] = "Native crash recorded\nRecord: boot 1 / sequence 1\nException PC 12345678\n";
    static const char cold_reuse[] = "Native crash recorded\nRecord: boot 1 / sequence 1\nException PC 87654321\n";
    static const char recovery_a[] = "Native crash recorded\nRecord: boot 1 / sequence 1\nThis boot reset reason: 3 (raw)\n";
    static const char recovery_b[] = "Native crash recorded\nRecord: boot 1 / sequence 1\nThis boot reset reason: 12 (raw)\n";
    reset(); crash_report_spool_identity id = identity(0);
    assert(append(0, first) == CRASH_REPORT_SPOOL_COMMITTED);
    /* A cold boot can reset native counters without changing firmware. */
    backend.revision = 93001;
    assert(crash_report_spool_init(&spool, &api) == CRASH_REPORT_SPOOL_OK);
    assert(append(0, cold_reuse) == CRASH_REPORT_SPOOL_COMMITTED && spool.count == 2);
    assert_entry(0, 0, first); assert_entry(1, 0, cold_reuse);
    assert(append(0, recovery_a) == CRASH_REPORT_SPOOL_COMMITTED);
    /* Pending evidence replayed after a warm reset has new recovery metadata. */
    backend.revision = 94001;
    assert(crash_report_spool_init(&spool, &api) == CRASH_REPORT_SPOOL_OK);
    assert(append(0, recovery_b) == CRASH_REPORT_SPOOL_COMMITTED && spool.count == 4);
    assert_entry(2, 0, recovery_a); assert_entry(3, 0, recovery_b);
    unsigned writes = backend.writes;
    assert(append(0, recovery_b) == CRASH_REPORT_SPOOL_COMMITTED && backend.writes == writes);
    assert(append(0, cold_reuse) == CRASH_REPORT_SPOOL_COMMITTED && backend.writes == writes);
    assert(crash_report_spool_remove(&spool, &id, "not saved", 9) == CRASH_REPORT_SPOOL_REMOVED);
    assert(spool.count == 4 && backend.writes == writes);
    /* Removing a later exact match must scan past earlier same-ID variants. */
    assert(crash_report_spool_remove(&spool, &id, recovery_b, sizeof(recovery_b) - 1) == CRASH_REPORT_SPOOL_REMOVED);
    assert(spool.count == 3); assert_entry(0, 0, first); assert_entry(1, 0, cold_reuse); assert_entry(2, 0, recovery_a);
    writes = backend.writes;
    assert(crash_report_spool_remove(&spool, &id, recovery_b, sizeof(recovery_b) - 1) == CRASH_REPORT_SPOOL_REMOVED);
    assert(spool.count == 3 && backend.writes == writes);
    assert(crash_report_spool_reload(&rebooted) == CRASH_REPORT_SPOOL_OK && rebooted.count == 3);
    /* Ambiguous removal verifies the requested text variant, not identity. */
    backend.write_fault = RISC_APP_DATA_COMMIT_UNKNOWN;
    assert(crash_report_spool_remove(&spool, &id, cold_reuse, sizeof(cold_reuse) - 1) == CRASH_REPORT_SPOOL_RETRY);
    assert(spool.count == 3); assert_entry(1, 0, cold_reuse);
    backend.write_fault = RISC_APP_DATA_COMMIT_UNKNOWN; backend.commit_on_fault = true;
    assert(crash_report_spool_remove(&spool, &id, cold_reuse, sizeof(cold_reuse) - 1) == CRASH_REPORT_SPOOL_REMOVED);
    assert(spool.count == 2); assert_entry(0, 0, first); assert_entry(1, 0, recovery_a);
    /* Overflow still preserves the first seven variants and records a drop. */
    for (unsigned i = 0; i < 6; ++i) {
        char text[32]; snprintf(text, sizeof(text), "same capture / variant %u\n", i);
        assert(append(0, text) == CRASH_REPORT_SPOOL_COMMITTED);
    }
    assert(spool.count == 8 && !spool.dropped_count);
    assert(append(0, recovery_b) == CRASH_REPORT_SPOOL_COMMITTED && spool.count == 8 && spool.dropped_count == 1);
    assert_entry(0, 0, first); assert_entry(1, 0, recovery_a); assert_entry(7, 0, recovery_b);
    ++tests;
}
static void test_text_bounds(void) {
    reset(); crash_report_spool_identity id = identity(0);
    memset(maximum_text, 'x', sizeof(maximum_text));
    assert(crash_report_spool_append(&spool, &id, maximum_text, CRASH_REPORT_SPOOL_TEXT_MAX + 1) == CRASH_REPORT_SPOOL_INVALID);
    assert(crash_report_spool_append(&spool, &id, NULL, 1) == CRASH_REPORT_SPOOL_INVALID);
    assert(crash_report_spool_append(&spool, &id, "", 0) == CRASH_REPORT_SPOOL_INVALID);
    assert(crash_report_spool_append(&spool, NULL, "ok", 2) == CRASH_REPORT_SPOOL_INVALID);
    static const char *invalid[] = {"a\0b", "\x01", "\x7f", "\x80", "\xc0\x80", "\xc2", "\xe0\x80\x80",
        "\xed\xa0\x80", "\xe2\x28\xa1", "\xf0\x80\x80\x80", "\xf4\x90\x80\x80", "\xf5\x80\x80\x80"};
    for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        uint32_t size = i ? (uint32_t)strlen(invalid[i]) : 3;
        assert(crash_report_spool_append(&spool, &id, invalid[i], size) == CRASH_REPORT_SPOOL_INVALID);
    }
    assert(!backend.stats && !backend.reads && !backend.writes);
    const char utf8[] = "\tASCII\r\n\xc2\x80\xdf\xbf\xe0\xa0\x80\xed\x9f\xbf\xee\x80\x80\xf0\x90\x80\x80\xf4\x8f\xbf\xbf\n";
    assert(crash_report_spool_append(&spool, &id, utf8, sizeof(utf8) - 1) == CRASH_REPORT_SPOOL_COMMITTED);
    id = identity(1);
    assert(crash_report_spool_append(&spool, &id, maximum_text, CRASH_REPORT_SPOOL_TEXT_MAX) == CRASH_REPORT_SPOOL_COMMITTED);
    assert(spool.count == 2);
    ++tests;
}
static void test_full_queue_drop_and_saturation(void) {
    reset(); memset(maximum_text, 'x', CRASH_REPORT_SPOOL_TEXT_MAX); maximum_text[CRASH_REPORT_SPOOL_TEXT_MAX] = 0;
    for (uint32_t i = 0; i < 8; ++i) assert(append(i, maximum_text) == CRASH_REPORT_SPOOL_COMMITTED);
    assert(backend.size == CRASH_REPORT_SPOOL_FILE_MAX && backend.size < 65536);
    assert(spool.count == 8 && !spool.dropped_count);
    assert(append(8, "newest\n") == CRASH_REPORT_SPOOL_COMMITTED);
    for (uint32_t i = 0; i < 7; ++i) assert_entry(i, i, maximum_text);
    assert_entry(7, 8, "newest\n");
    assert(spool.count == 8 && spool.dropped_count == 1);
    crash_report_spool_identity dropped = identity(7);
    assert(!memcmp(spool.last_dropped_identity.firmware_sha256, dropped.firmware_sha256, 32));
    assert(spool.last_dropped_identity.captured_boot == dropped.captured_boot);
    assert(spool.last_dropped_identity.captured_sequence == dropped.captured_sequence);
    unsigned writes = backend.writes;
    assert(append(8, "newest\n") == CRASH_REPORT_SPOOL_COMMITTED && backend.writes == writes && spool.dropped_count == 1);
    assert(append(9, maximum_text) == CRASH_REPORT_SPOOL_COMMITTED);
    assert(spool.dropped_count == 2 && spool.last_dropped_identity.captured_sequence == identity(8).captured_sequence);
    assert(backend.size == CRASH_REPORT_SPOOL_FILE_MAX);
    /* A discarded report is not a dedupe success. Failed re-add is RETRY. */
    backend.write_fault = RISC_APP_DATA_NO_SPACE;
    assert(append(7, maximum_text) == CRASH_REPORT_SPOOL_RETRY && !spool.loaded);
    assert(crash_report_spool_reload(&spool) == CRASH_REPORT_SPOOL_OK && spool.dropped_count == 2);
    assert_entry(7, 9, maximum_text);
    crash_report_spool_put32(backend.bytes + 20, UINT32_MAX - 1); repair_crc();
    assert(append(10, "ten\n") == CRASH_REPORT_SPOOL_COMMITTED && spool.dropped_count == UINT32_MAX);
    assert(append(11, "eleven\n") == CRASH_REPORT_SPOOL_COMMITTED && spool.dropped_count == UINT32_MAX);
    assert(spool.last_dropped_identity.captured_sequence == identity(10).captured_sequence);
    assert(crash_report_spool_reload(&rebooted) == CRASH_REPORT_SPOOL_OK && rebooted.dropped_count == UINT32_MAX);
    for (unsigned i = 0; i < 8; ++i) {
        crash_report_spool_view view;
        assert(crash_report_spool_peek(&spool, 0, &view) == CRASH_REPORT_SPOOL_OK);
        assert(crash_report_spool_remove(&spool, &view.identity, view.text, view.text_size) == CRASH_REPORT_SPOOL_REMOVED);
    }
    assert(!spool.count && spool.present && spool.dropped_count == UINT32_MAX);
    assert(backend.size == CRASH_REPORT_SPOOL_HEADER_SIZE);
    assert(crash_report_spool_reload(&rebooted) == CRASH_REPORT_SPOOL_OK && !rebooted.count && rebooted.dropped_count == UINT32_MAX);
    assert(rebooted.last_dropped_identity.captured_sequence == identity(10).captured_sequence);
    ++tests;
}
static void test_non_mutating_failures(void) {
    static const int32_t retry_errors[] = {RISC_APP_DATA_UNAVAILABLE, RISC_APP_DATA_IO, RISC_APP_DATA_NO_SPACE,
        RISC_APP_DATA_STALE, RISC_APP_DATA_COMMIT_UNKNOWN, RISC_APP_DATA_NOT_FOUND};
    for (unsigned i = 0; i < sizeof(retry_errors) / sizeof(retry_errors[0]); ++i) {
        reset(); backend.stat_fault = retry_errors[i];
        if (retry_errors[i] != RISC_APP_DATA_NOT_FOUND) {
            assert(append(0, "report\n") == CRASH_REPORT_SPOOL_RETRY);
            assert(!backend.reads && !backend.writes && !backend.present && !spool.loaded);
        }
        reset(); assert(append(0, "original\n") == CRASH_REPORT_SPOOL_COMMITTED); save_disk();
        backend.read_fault = retry_errors[i]; unsigned writes = backend.writes;
        assert(append(1, "next\n") == CRASH_REPORT_SPOOL_RETRY);
        assert(backend.writes == writes && !spool.loaded); assert_unchanged();
        backend.write_fault = retry_errors[i];
        unsigned stats = backend.stats;
        assert(append(1, "next\n") == CRASH_REPORT_SPOOL_RETRY);
        unsigned reconciles = (retry_errors[i] == RISC_APP_DATA_STALE || retry_errors[i] == RISC_APP_DATA_COMMIT_UNKNOWN) ? 1u : 0u;
        assert(backend.stats == stats + 1 + reconciles && backend.writes == writes + 1);
        assert_unchanged();
        assert(crash_report_spool_reload(&spool) == CRASH_REPORT_SPOOL_OK);
        assert_entry(0, 0, "original\n");
    }
    reset(); assert(append(0, "old\n") == CRASH_REPORT_SPOOL_COMMITTED);
    backend.present = false; unsigned writes = backend.writes;
    assert(append(1, "next\n") == CRASH_REPORT_SPOOL_UNSAFE && backend.writes == writes);
    ++tests;
}
static void test_append_ambiguity_and_restart(void) {
    static const int32_t errors[] = {RISC_APP_DATA_STALE, RISC_APP_DATA_COMMIT_UNKNOWN};
    for (unsigned e = 0; e < 2; ++e) for (unsigned old = 0; old < 2; ++old) for (unsigned existed = 0; existed < 2; ++existed) {
        reset(); if (existed) assert(append(0, "old\n") == CRASH_REPORT_SPOOL_COMMITTED);
        backend.write_fault = errors[e]; backend.commit_on_fault = !old;
        unsigned stats = backend.stats, writes = backend.writes;
        assert(append(1, "new\n") == (old ? CRASH_REPORT_SPOOL_RETRY : CRASH_REPORT_SPOOL_COMMITTED));
        assert(backend.stats == stats + 2 && backend.writes == writes + 1);
        assert(spool.last_app_data_error == errors[e]);
        assert(crash_report_spool_reload(&rebooted) == CRASH_REPORT_SPOOL_OK);
        assert(rebooted.count == existed + !old);
        assert(append(1, "new\n") == CRASH_REPORT_SPOOL_COMMITTED);
        assert(backend.writes == writes + 1 + old);
    }
    reset(); assert(append(0, "old\n") == CRASH_REPORT_SPOOL_COMMITTED);
    backend.write_fault = RISC_APP_DATA_COMMIT_UNKNOWN; backend.commit_on_fault = true;
    backend.after_write_read_fault = RISC_APP_DATA_IO;
    unsigned writes = backend.writes;
    assert(append(1, "new\n") == CRASH_REPORT_SPOOL_RETRY && !spool.loaded && backend.writes == writes + 1);
    assert(crash_report_spool_reload(&rebooted) == CRASH_REPORT_SPOOL_OK && rebooted.count == 2);
    crash_report_spool_identity id = identity(1);
    assert(crash_report_spool_append(&rebooted, &id, "new\n", 4) == CRASH_REPORT_SPOOL_COMMITTED && backend.writes == writes + 1);
    reset(); backend.write_fault = RISC_APP_DATA_COMMIT_UNKNOWN; backend.commit_on_fault = true; backend.collision_on_fault = true;
    assert(append(0, "alpha\n") == CRASH_REPORT_SPOOL_RETRY && spool.loaded);
    assert_entry(0, 0, "Zlpha\n");
    assert(append(0, "alpha\n") == CRASH_REPORT_SPOOL_COMMITTED && spool.count == 2);
    assert_entry(0, 0, "Zlpha\n"); assert_entry(1, 0, "alpha\n");
    reset(); backend.write_fault = RISC_APP_DATA_COMMIT_UNKNOWN; backend.commit_on_fault = true; backend.corrupt_on_fault = true;
    assert(append(0, "alpha\n") == CRASH_REPORT_SPOOL_CORRUPT && !spool.loaded);
    writes = backend.writes;
    assert(append(1, "beta\n") == CRASH_REPORT_SPOOL_CORRUPT && backend.writes == writes);
    /* Overflow accounting follows whichever complete file actually survives. */
    reset(); for (unsigned i = 0; i < 8; ++i) assert(append(i, "report\n") == CRASH_REPORT_SPOOL_COMMITTED);
    backend.write_fault = RISC_APP_DATA_COMMIT_UNKNOWN;
    assert(append(8, "new\n") == CRASH_REPORT_SPOOL_RETRY && spool.dropped_count == 0);
    assert_entry(7, 7, "report\n");
    backend.write_fault = RISC_APP_DATA_COMMIT_UNKNOWN; backend.commit_on_fault = true;
    assert(append(8, "new\n") == CRASH_REPORT_SPOOL_COMMITTED && spool.dropped_count == 1);
    assert(append(8, "new\n") == CRASH_REPORT_SPOOL_COMMITTED && spool.dropped_count == 1);
    ++tests;
}
static void test_remove_success_failures_ambiguity(void) {
    static const int32_t errors[] = {RISC_APP_DATA_IO, RISC_APP_DATA_NO_SPACE, RISC_APP_DATA_UNAVAILABLE,
        RISC_APP_DATA_STALE, RISC_APP_DATA_COMMIT_UNKNOWN};
    crash_report_spool_identity id = identity(1);
    for (unsigned e = 0; e < sizeof(errors) / sizeof(errors[0]); ++e) {
        reset(); assert(append(0, "first\n") == CRASH_REPORT_SPOOL_COMMITTED);
        assert(append(1, "middle\n") == CRASH_REPORT_SPOOL_COMMITTED);
        assert(append(2, "last\n") == CRASH_REPORT_SPOOL_COMMITTED); save_disk();
        backend.write_fault = errors[e]; unsigned writes = backend.writes;
        assert(crash_report_spool_remove(&spool, &id, "middle\n", 7) == CRASH_REPORT_SPOOL_RETRY);
        assert(backend.writes == writes + 1); assert_unchanged();
        assert(crash_report_spool_reload(&rebooted) == CRASH_REPORT_SPOOL_OK && rebooted.count == 3);
        if (errors[e] == RISC_APP_DATA_STALE || errors[e] == RISC_APP_DATA_COMMIT_UNKNOWN) {
            backend.write_fault = errors[e]; backend.commit_on_fault = true;
        }
        assert(crash_report_spool_remove(&spool, &id, "middle\n", 7) == CRASH_REPORT_SPOOL_REMOVED);
        assert(spool.count == 2); assert_entry(0, 0, "first\n"); assert_entry(1, 2, "last\n");
        writes = backend.writes;
        assert(crash_report_spool_remove(&spool, &id, "middle\n", 7) == CRASH_REPORT_SPOOL_REMOVED && backend.writes == writes);
    }
    reset(); assert(append(1, "middle\n") == CRASH_REPORT_SPOOL_COMMITTED);
    backend.write_fault = RISC_APP_DATA_COMMIT_UNKNOWN; backend.commit_on_fault = true;
    backend.after_write_stat_fault = RISC_APP_DATA_UNAVAILABLE;
    assert(crash_report_spool_remove(&spool, &id, "middle\n", 7) == CRASH_REPORT_SPOOL_RETRY && !spool.loaded);
    assert(crash_report_spool_reload(&rebooted) == CRASH_REPORT_SPOOL_OK && !rebooted.count && rebooted.present);
    unsigned writes = backend.writes;
    assert(crash_report_spool_remove(&rebooted, &id, "middle\n", 7) == CRASH_REPORT_SPOOL_REMOVED && backend.writes == writes);
    reset();
    assert(crash_report_spool_remove(&spool, &id, "middle\n", 7) == CRASH_REPORT_SPOOL_REMOVED && !backend.writes && !backend.present);
    ++tests;
}
static void test_corrupt_crc_and_lengths(void) {
    reset(); assert(append(0, "a\nb\n") == CRASH_REPORT_SPOOL_COMMITTED); save_disk();
    for (uint32_t i = 0; i < saved_size; ++i) {
        restore_disk(); backend.bytes[i] ^= 1u; assert_corrupt_no_write();
    }
    for (uint32_t size = 0; size < saved_size; ++size) {
        restore_disk(); backend.size = size; assert_corrupt_no_write();
    }
    restore_disk(); backend.size = CRASH_REPORT_SPOOL_FILE_MAX + 1; assert_corrupt_no_write();
    restore_disk(); backend.size = RISC_APP_DATA_FILE_MAX; assert_corrupt_no_write();
    static const struct { unsigned offset; uint32_t value; } mutations[] = {
        {0, 0}, {8, 2}, {10, 79}, {12, 133}, {12, UINT32_MAX}, {16, 9}, {16, UINT32_MAX},
        {24, 1}, {64, 1}, {72, 1}, {120, 0}, {120, 4097}, {120, UINT32_MAX}, {124, 1}
    };
    for (unsigned i = 0; i < sizeof(mutations) / sizeof(mutations[0]); ++i) {
        restore_disk(); crash_report_spool_put32(backend.bytes + mutations[i].offset, mutations[i].value);
        repair_crc(); assert_corrupt_no_write();
    }
    restore_disk(); backend.bytes[128] = 0; repair_crc(); assert_corrupt_no_write();
    restore_disk(); backend.bytes[128] = 0xff; repair_crc(); assert_corrupt_no_write();
    restore_disk(); ++backend.size; backend.bytes[backend.size - 1] = 0;
    crash_report_spool_put32(backend.bytes + 12, backend.size); repair_crc(); assert_corrupt_no_write();
    restore_disk(); memcpy(backend.bytes + backend.size, backend.bytes + 80, backend.size - 80);
    backend.size += backend.size - 80;
    crash_report_spool_put32(backend.bytes + 12, backend.size); crash_report_spool_put32(backend.bytes + 16, 2);
    repair_crc(); assert_corrupt_no_write(); /* Exact identity+text duplicates are not canonical. */
    ++tests;
}
static void test_protocol_unsafe(void) {
    static const int32_t errors[] = {RISC_APP_DATA_CONTEXT, RISC_APP_DATA_INVALID, RISC_APP_DATA_BUFFER_SMALL, -123};
    for (unsigned i = 0; i < sizeof(errors) / sizeof(errors[0]); ++i) {
        reset(); backend.stat_fault = errors[i];
        assert(append(0, "report\n") == CRASH_REPORT_SPOOL_UNSAFE && !backend.writes);
        reset(); assert(append(0, "report\n") == CRASH_REPORT_SPOOL_COMMITTED);
        unsigned writes = backend.writes; backend.read_fault = errors[i];
        assert(append(1, "new\n") == CRASH_REPORT_SPOOL_UNSAFE && backend.writes == writes);
        backend.write_fault = errors[i];
        assert(append(1, "new\n") == CRASH_REPORT_SPOOL_UNSAFE && !spool.loaded);
    }
    reset(); backend.bad_stat_absence = true;
    assert(append(0, "report\n") == CRASH_REPORT_SPOOL_UNSAFE && !backend.writes);
    reset(); assert(append(0, "report\n") == CRASH_REPORT_SPOOL_COMMITTED);
    unsigned writes = backend.writes;
    backend.bad_stat_revision = true;
    assert(append(1, "new\n") == CRASH_REPORT_SPOOL_UNSAFE && backend.writes == writes);
    backend.bad_stat_revision = false; backend.bad_read_size = true;
    assert(append(1, "new\n") == CRASH_REPORT_SPOOL_UNSAFE && backend.writes == writes);
    backend.bad_read_size = false; backend.bad_read_revision = true;
    assert(append(1, "new\n") == CRASH_REPORT_SPOOL_UNSAFE && backend.writes == writes);
    reset(); risc_app_data_v1 bad = api; bad.struct_size = sizeof(api) - 1;
    assert(crash_report_spool_init(&spool, &bad) == CRASH_REPORT_SPOOL_INVALID);
    assert(crash_report_spool_reload(&spool) == CRASH_REPORT_SPOOL_UNSAFE && !backend.stats);
    bad = api; bad.api_version = 2;
    assert(crash_report_spool_init(&spool, &bad) == CRASH_REPORT_SPOOL_INVALID);
    bad = api; bad.replace = NULL;
    assert(crash_report_spool_init(&spool, &bad) == CRASH_REPORT_SPOOL_INVALID);
    assert(crash_report_spool_init(NULL, &api) == CRASH_REPORT_SPOOL_INVALID);
    assert(crash_report_spool_init(&spool, NULL) == CRASH_REPORT_SPOOL_INVALID);
    assert(crash_report_spool_reload(NULL) == CRASH_REPORT_SPOOL_INVALID);
    assert(crash_report_spool_peek(NULL, 0, NULL) == CRASH_REPORT_SPOOL_INVALID);
    assert(crash_report_spool_append(NULL, NULL, NULL, 0) == CRASH_REPORT_SPOOL_INVALID);
    assert(crash_report_spool_remove(NULL, NULL, NULL, 0) == CRASH_REPORT_SPOOL_INVALID);
    ++tests;
}
static void test_retained_terminal(void) {
    for (unsigned phase = 0; phase < 5; ++phase) {
        reset(); assert(append(0, "old\n") == CRASH_REPORT_SPOOL_COMMITTED);
        if (phase == 0) backend.stat_fault = RISC_APP_DATA_RETAINED;
        if (phase == 1) backend.read_fault = RISC_APP_DATA_RETAINED;
        if (phase == 2) backend.write_fault = RISC_APP_DATA_RETAINED;
        if (phase >= 3) {
            backend.write_fault = RISC_APP_DATA_COMMIT_UNKNOWN; backend.commit_on_fault = true;
            if (phase == 3) backend.after_write_stat_fault = RISC_APP_DATA_RETAINED;
            else backend.after_write_read_fault = RISC_APP_DATA_RETAINED;
        }
        assert(append(1, "new\n") == CRASH_REPORT_SPOOL_RETAINED);
        assert(spool.retained && !spool.loaded);
        unsigned calls = backend.stats + backend.reads + backend.writes;
        crash_report_spool_identity id = identity(0); crash_report_spool_view view;
        assert(crash_report_spool_reload(&spool) == CRASH_REPORT_SPOOL_RETAINED);
        assert(crash_report_spool_peek(&spool, 0, &view) == CRASH_REPORT_SPOOL_RETAINED);
        assert(crash_report_spool_append(&spool, &id, "old\n", 4) == CRASH_REPORT_SPOOL_RETAINED);
        assert(crash_report_spool_remove(&spool, &id, "old\n", 4) == CRASH_REPORT_SPOOL_RETAINED);
        assert(crash_report_spool_append(&spool, NULL, NULL, 0) == CRASH_REPORT_SPOOL_RETAINED);
        assert(crash_report_spool_remove(&spool, NULL, NULL, 0) == CRASH_REPORT_SPOOL_RETAINED);
        assert(backend.stats + backend.reads + backend.writes == calls);
        /* Explicit simulated runtime restart gives fresh state and callbacks. */
        ++backend.revision;
        assert(crash_report_spool_init(&rebooted, &api) == CRASH_REPORT_SPOOL_OK);
        assert(crash_report_spool_reload(&rebooted) == CRASH_REPORT_SPOOL_OK && !rebooted.retained);
    }
    ++tests;
}
static uint32_t random_state = UINT32_C(0x2c6771da);
static uint32_t random_u32(void) {
    random_state ^= random_state << 13; random_state ^= random_state >> 17; random_state ^= random_state << 5;
    return random_state;
}
static void test_structure_fuzz(void) {
    reset(); assert(append(0, "a\nb\n") == CRASH_REPORT_SPOOL_COMMITTED); save_disk();
    for (unsigned run = 0; run < 1200; ++run) {
        restore_disk();
        unsigned choice = random_u32() % 7;
        uint32_t value = random_u32();
        if (choice == 0) crash_report_spool_put32(backend.bytes + 16, value | 16u);
        if (choice == 1) crash_report_spool_put32(backend.bytes + 120, value | 8192u);
        if (choice == 2) crash_report_spool_put32(backend.bytes + 124, value | 1u);
        if (choice == 3) crash_report_spool_put32(backend.bytes + 64, value | 1u);
        if (choice == 4) backend.bytes[128 + random_u32() % 4] = 0;
        if (choice == 5) crash_report_spool_put32(backend.bytes + 24, value | 1u);
        if (choice == 6) crash_report_spool_put32(backend.bytes + 12, value | 256u);
        repair_crc(); assert_corrupt_no_write();
    }
    ++tests;
}
int main(void) {
    test_first_queue_encoding_restart();
    test_exact_dedupe_variants();
    test_reused_capture_and_recovery_reset_variants();
    test_text_bounds();
    test_full_queue_drop_and_saturation();
    test_non_mutating_failures();
    test_append_ambiguity_and_restart();
    test_remove_success_failures_ambiguity();
    test_corrupt_crc_and_lengths();
    test_protocol_unsafe();
    test_retained_terminal();
    test_structure_fuzz();
    printf("Crash report spool: %u groups PASS; canonical LE/CRC, bounded UTF-8, exact dedupe/text variants, oldest-seven/newest overflow, saturation, stale/ambiguous recovery, removal, reboot, corruption, terminal retained; state=%zu file_max=%u\n",
           tests, sizeof(spool), (unsigned)CRASH_REPORT_SPOOL_FILE_MAX);
    return 0;
}

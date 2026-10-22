/* Test-only software serial provider for the unchanged monitor's real Runtime
 * integration. No physical transport, production catalog, or client API double.
 * Build separately for each explicit catalog instance: the decoy gets its own
 * module data/BSS and cannot satisfy the monitor's instance-41 grant. */
#include <RiscHardwareConfigV1.h>
#include <RiscSerialPortV1.h>
#include <RiscSerialStreamSessionV1.h>
#include <RiscStreamSessionProviderV1.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "bridge_fixture.h"

#ifndef FIXTURE_SERIAL_INSTANCE
#error "Build this provider with its explicit fixture catalog instance"
#endif
#if FIXTURE_SERIAL_INSTANCE != 41 && FIXTURE_SERIAL_INSTANCE != 42
#error "Only the selected instance 41 and decoy instance 42 are fixture inputs"
#endif
#define STRING_VALUE_(value) #value
#define STRING_VALUE(value) STRING_VALUE_(value)
#define EVENT_PREFIX "serial:" STRING_VALUE(FIXTURE_SERIAL_INSTANCE) ":"
#define DEVICE UINT64_C(17)
#define GENERATION UINT64_C(9)
#define QUEUE_CAPACITY 128u
#define RX_CHUNK 7u

static const risc_stream_provider_v1 *host;
static uint64_t sequence, session;
static uint32_t rx_endpoint, tx_endpoint;
static size_t rx_offset;
static bool started, stopped;
static risc_serial_stream_config_v1 framing;
static const uint8_t received[] = "Ab9 !?[]{}=+-_/\\\r\nRX COMPLETE\r\n";

static void event(const char *name) {
    char line[128];
    const int size = snprintf(line, sizeof(line), EVENT_PREFIX "%s", name);
    if (size > 0 && (size_t)size < sizeof(line)) monitor_fixture_event(line);
}
static bool mode_is(const char *name) {
    const char *mode = monitor_fixture_mode();
    return mode && !strcmp(mode, name);
}
static bool valid_budget(uint32_t ms) { return ms && ms <= 1000u; }
static bool valid_config(const risc_serial_stream_config_v1 *config) {
    return config && config->baud_rate >= 300 && config->baud_rate <= 3000000 &&
        config->data_bits >= 5 && config->data_bits <= 8 && config->parity <= 4 &&
        (config->stop_bits == 1 || config->stop_bits == 2) && !config->flow_control;
}
static bool expected_config(const risc_serial_stream_config_v1 *config) {
    return config && config->baud_rate == 115200 && config->data_bits == 8 &&
        config->parity == 0 && config->stop_bits == 1 && config->flow_control == 0;
}
static bool snapshot(risc_serial_device_v1 *out, size_t *count) {
    event("snapshot");
    if (!count) return false;
    if (!out || *count < 1) { *count = 1; return false; }
    out[0] = (risc_serial_device_v1){DEVICE, GENERATION,
        RISC_SERIAL_TRANSPORT_INTERNAL, {0}};
    *count = 1;
    return true;
}
static int32_t check_device(uint64_t device, uint64_t generation) {
    return device == DEVICE && generation == GENERATION ?
        RISC_STREAM_OK : RISC_STREAM_DISCONNECTED;
}

/* Closure is bounded and synchronous. Stop the pump before revoking queues;
 * never pretend that an uncertain close proves the session has disappeared. */
static int32_t close_queues(void) {
    stopped = true;
    if ((rx_endpoint && host->finish(host->context, rx_endpoint,
            RISC_STREAM_CANCELLED) != RISC_STREAM_OK) ||
        (tx_endpoint && host->finish(host->context, tx_endpoint,
            RISC_STREAM_CANCELLED) != RISC_STREAM_OK))
        return RISC_STREAM_RETAINED;
    if ((rx_endpoint && host->close(host->context, rx_endpoint) != RISC_STREAM_OK) ||
        (tx_endpoint && host->close(host->context, tx_endpoint) != RISC_STREAM_OK))
        return RISC_STREAM_RETAINED;
    session = 0;
    rx_endpoint = tx_endpoint = 0;
    return RISC_STREAM_OK;
}
static int32_t close_session(uint64_t token, uint32_t ms) {
    event("close");
    if (!valid_budget(ms) || !token || token != session) return RISC_STREAM_INVALID;
    stopped = true;
    if (mode_is("close-retained")) {
        /* Retain the exact live token and queues. Runtime supplies the fence;
         * this fixture adds no retention API or alternative cleanup owner. */
        event("close-retained");
        event("terminal");
        return RISC_STREAM_RETAINED;
    }
    const int32_t result = close_queues();
    if (result != RISC_STREAM_OK) { event("terminal"); return result; }
    event("close-ok");
    if (mode_is("close-overrun")) {
        event("close-overrun");
        /* Host AppStreamSessions measures steady_clock, not the UI fake clock.
         * Return OK only after a real delay, so its existing elapsed-time
         * check, rather than a provider RETAINED result, causes the fence. */
        monitor_fixture_delay(ms + 30u);
        event("terminal");
    }
    return RISC_STREAM_OK;
}
static int32_t open_session(const void *bytes, uint32_t size, uint32_t ms,
        risc_provider_stream_session_v1 *out) {
    event("open");
    if (!out || out->struct_size < sizeof(*out)) return RISC_STREAM_INVALID;
    *out = (risc_provider_stream_session_v1){.struct_size = sizeof(*out)};
    if (!started || !host || !bytes || size != sizeof(risc_serial_stream_open_v1) ||
        !valid_budget(ms)) return RISC_STREAM_INVALID;
    risc_serial_stream_open_v1 request;
    memcpy(&request, bytes, sizeof(request));
    if (request.api_version != RISC_SERIAL_STREAM_REQUEST_V1 ||
        request.struct_size != sizeof(request) || !valid_config(&request.config))
        return RISC_STREAM_INVALID;
    assert(expected_config(&request.config));
    const int32_t present = check_device(request.provider_device, request.device_generation);
    if (present != RISC_STREAM_OK) return present;
    if (session) return RISC_STREAM_BUSY;
    if (sequence == UINT64_MAX) return RISC_STREAM_LIMIT;
    session = ++sequence;
    stopped = false;
    rx_offset = 0;
    risc_stream_endpoint_v1 endpoint = {sizeof(endpoint), 1,
        RISC_STREAM_READ, QUEUE_CAPACITY, NULL, 0, 0};
    if (host->publish(host->context, &endpoint, &rx_endpoint) != RISC_STREAM_OK)
        goto rollback;
    endpoint.rights = RISC_STREAM_WRITE;
    if (host->publish(host->context, &endpoint, &tx_endpoint) != RISC_STREAM_OK)
        goto rollback;
    framing = request.config;
    out->session = session;
    out->rx_endpoint = rx_endpoint;
    out->tx_endpoint = tx_endpoint;
    event("open-ok");
    return RISC_STREAM_OK;
rollback:
    if (close_queues() == RISC_STREAM_OK) return RISC_STREAM_LIMIT;
    out->session = session;
    out->rx_endpoint = rx_endpoint;
    out->tx_endpoint = tx_endpoint;
    event("terminal");
    return RISC_STREAM_RETAINED;
}
static int32_t call_session(uint64_t token, const void *bytes, uint32_t size,
        uint32_t ms, void *reply, uint32_t capacity, uint32_t *actual) {
    (void)reply;
    (void)capacity;
    if (actual) *actual = 0;
    if (!actual || !token || token != session || !bytes ||
        size != sizeof(risc_serial_stream_call_v1) || !valid_budget(ms))
        return RISC_STREAM_INVALID;
    risc_serial_stream_call_v1 request;
    memcpy(&request, bytes, sizeof(request));
    if (request.api_version != RISC_SERIAL_STREAM_REQUEST_V1 ||
        request.struct_size != sizeof(request) || request.reserved)
        return RISC_STREAM_INVALID;
    switch (request.operation) {
    case RISC_SERIAL_STREAM_CHECK_DEVICE:
        for (size_t i = 0; i < sizeof(request.value.zero); ++i)
            if (request.value.zero[i]) return RISC_STREAM_INVALID;
        event("check");
        if (mode_is("call-overrun")) {
            event("call-overrun");
            monitor_fixture_delay(ms + 30u);
            event("terminal");
        }
        return RISC_STREAM_OK;
    case RISC_SERIAL_STREAM_CONFIGURE:
        if (!valid_config(&request.value.config)) return RISC_STREAM_INVALID;
        assert(expected_config(&request.value.config));
        framing = request.value.config;
        event("configure");
        return RISC_STREAM_OK;
    case RISC_SERIAL_STREAM_CONTROL_LINES:
        if (request.value.lines.dtr > 1 || request.value.lines.rts > 1)
            return RISC_STREAM_INVALID;
        for (size_t i = 0; i < sizeof(request.value.lines.reserved); ++i)
            if (request.value.lines.reserved[i]) return RISC_STREAM_INVALID;
        event("lines");
        return RISC_STREAM_OK;
    default:
        return RISC_STREAM_UNSUPPORTED;
    }
}
static bool bind_streams(const risc_stream_provider_v1 *table) {
    event("bind");
    if (!table || table->api_version != RISC_STREAM_PROVIDER_API_V1 ||
        table->struct_size < sizeof(*table) || !table->context || !table->publish ||
        !table->produce || !table->consume || !table->finish || !table->close)
        return false;
    host = table;
    return true;
}
static bool start(const risc_provider_dependency_v1 *dependencies, size_t count) {
    event("start");
    if (!host || !dependencies || count != 1 || !dependencies[0].capability_id ||
        strcmp(dependencies[0].capability_id, "hardware.device") ||
        dependencies[0].api_version != RISC_HARDWARE_DEVICE_API_V1 ||
        !dependencies[0].api) return false;
    const risc_hardware_device_v1 *device = dependencies[0].api;
    if (device->api_version != RISC_HARDWARE_DEVICE_API_V1 ||
        device->struct_size != sizeof(*device) ||
        device->instance_id != FIXTURE_SERIAL_INSTANCE ||
        !device->compatible || strcmp(device->compatible, "monitor-fixture,serial") ||
        !device->revision || strcmp(device->revision, "unspecified") ||
        !device->config_type || strcmp(device->config_type, "gpio.bank") ||
        device->config_version != 1 || device->config_size != sizeof(risc_hw_gpio_bank_v1) ||
        !device->config) return false;
    const risc_hw_gpio_bank_v1 *config = device->config;
    if (config->struct_size != sizeof(*config)) return false;
    /* This materialized fixture identity grants no hardware operation. */
    started = true;
    event("start-ok");
    return true;
}
static bool quiesce(void) {
    event("quiesce");
    return !session && !rx_endpoint && !tx_endpoint;
}
static void stop(void) {
    event("stop");
    started = false;
    host = NULL;
}
static void poll(uint32_t ms) {
    event("poll");
    if (!ms || !started || !session || stopped) return;
    uint8_t unexpected;
    uint32_t transmitted = 0;
    const int32_t tx_status = host->consume(host->context, tx_endpoint, &unexpected, 1, &transmitted);
    assert((tx_status == RISC_STREAM_OK || tx_status == RISC_STREAM_AGAIN) && !transmitted);
    if (rx_offset == sizeof(received) - 1) return;
    uint32_t count = (uint32_t)(sizeof(received) - 1 - rx_offset);
    if (count > RX_CHUNK) count = RX_CHUNK;
    uint32_t written = 0;
    const int32_t result = host->produce(host->context, rx_endpoint,
        received + rx_offset, count, &written);
    if (result == RISC_STREAM_OK && written && written <= count) {
        char line[48];
        rx_offset += written;
        (void)snprintf(line, sizeof(line), "rx:%u", written);
        event(line);
        if (rx_offset == sizeof(received) - 1) event("rx-complete");
    } else if (result != RISC_STREAM_AGAIN) {
        event("rx-error");
    }
    /* The receive-only monitor never transmits; there is deliberately no
     * echo dependency, raw read, or alternative app-side queue here. TX is
     * inspected only as a zero-byte tripwire; it never triggers RX production. */
}

/* All raw-prefix callbacks are tripwires: this suffix path may snapshot the
 * inventory, but every session/byte transfer must go through real Runtime. */
static uint64_t raw_open(uint64_t device) {
    (void)device; event("raw:open"); return 0;
}
static int32_t raw_probe(uint64_t device) {
    (void)device; event("raw:probe"); return -1;
}
static bool raw_configure(uint64_t token, uint32_t baud, uint8_t bits,
        uint8_t parity, uint8_t stop_bits) {
    (void)token; (void)baud; (void)bits; (void)parity; (void)stop_bits;
    event("raw:configure"); return false;
}
static bool raw_lines(uint64_t token, bool dtr, bool rts) {
    (void)token; (void)dtr; (void)rts; event("raw:lines"); return false;
}
static int32_t raw_read(uint64_t token, uint8_t *bytes, size_t size, uint32_t ms) {
    (void)token; (void)bytes; (void)size; (void)ms; event("raw:read"); return -1;
}
static int32_t raw_write(uint64_t token, const uint8_t *bytes, size_t size, uint32_t ms) {
    (void)token; (void)bytes; (void)size; (void)ms; event("raw:write"); return -1;
}
static bool raw_close(uint64_t token) {
    (void)token; event("raw:close"); return false;
}
static bool raw_endpoints(uint64_t token, uint32_t *rx, uint32_t *tx) {
    (void)token; (void)rx; (void)tx; event("raw:endpoints"); return false;
}
static const risc_serial_port_streams_v1 serial = {
    .inventory = {
        .discovery = {.serial = {1, sizeof(serial), raw_open, raw_configure,
            raw_lines, raw_read, raw_write, raw_close}, .probe = raw_probe},
        .snapshot = snapshot
    },
    .endpoints = raw_endpoints
};
static const risc_stream_session_provider_v1 adapter = {
    1, sizeof(adapter), open_session, call_session, close_session
};
static const risc_driver_stream_sessions_v2 driver = {
    .poll = {
        .streams = {
            .driver = {2, sizeof(driver),
                "monitor-serial-" STRING_VALUE(FIXTURE_SERIAL_INSTANCE),
                "serial.port", 1, &serial, start, stop, quiesce},
            .last_error = NULL,
            .bind_streams = bind_streams
        },
        .poll = poll
    },
    .extension_tag = RISC_DRIVER_STREAM_SESSIONS_TAG_V1,
    .extension_version = RISC_DRIVER_STREAM_SESSIONS_VERSION_V1,
    .stream_sessions = &adapter
};
__attribute__((constructor)) static void loaded(void) { event("load"); }
__attribute__((destructor)) static void unloaded(void) { event("unload"); }
__attribute__((visibility("default")))
const risc_driver_v2 *t5_driver_get(uint32_t abi) {
    return abi == RISC_PROVIDER_DRIVER_ABI_V2 ? &driver.poll.streams.driver : NULL;
}

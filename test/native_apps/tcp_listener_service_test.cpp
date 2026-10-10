// Compile the actual provider unchanged. Direct access is used only to place its
// monotonic counters at overflow; lifecycle and data tests use the public ABI.
#include "../../Services/tcp_listener/service.cpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

namespace Fake {
void *const context = reinterpret_cast<void *>(uintptr_t(0x200));
const risc_driver_v2 *fake_driver;
risc_tcp_connection_v1 connection;
uint64_t native_generation = 1000;
unsigned calls, listen_calls, accept_calls, read_calls, write_calls, close_calls;
int32_t listen_result, accept_result, transfer_result, close_result;
uint32_t transfer_count = 3;
uint64_t error_handle, duplicate_handle;
unsigned fail_close_at = 1;
bool reenter, zero_handle;
const void *caller_buffer;
const void *caller_request;
risc_tcp_listen_v1 request_copy;
std::string written;
std::vector<uint64_t> closed;

void entered(void *value) {
    assert(value == context); ++calls;
    if (reenter) {
        uint64_t out = 9;
        risc_tcp_connection_listen_v1 request{sizeof(request), {127,0,0,1}, 8080, 0};
        assert(connection.listen(connection.context, &request, &out) == RISC_TCP_CONNECTION_BUSY);
        assert(out == 0 && !fake_driver->quiesce());
        fake_driver->stop();
    }
}
int32_t listen(void *value, const risc_tcp_listen_v1 *request, uint64_t *out) {
    entered(value); ++listen_calls;
    assert(request && static_cast<const void *>(request) != caller_request);
    request_copy = *request;
    *out = listen_result ? error_handle : (zero_handle ? 0 : (duplicate_handle ? duplicate_handle : ++native_generation));
    return listen_result;
}
int32_t accept(void *value, uint64_t handle, uint64_t *out) {
    entered(value); ++accept_calls; assert(handle >= 1001);
    *out = accept_result ? error_handle : (zero_handle ? 0 : (duplicate_handle ? duplicate_handle : ++native_generation));
    return accept_result;
}
int32_t read(void *value, uint64_t handle, void *bytes, uint32_t size, uint32_t *count) {
    entered(value); ++read_calls; assert(handle >= 1002);
    assert(bytes && bytes != caller_buffer && size <= RISC_TCP_BYTES_MAX);
    std::memset(bytes, 'r', size); *count = transfer_count; return transfer_result;
}
int32_t write(void *value, uint64_t handle, const void *bytes, uint32_t size, uint32_t *count) {
    entered(value); ++write_calls; assert(handle >= 1002);
    assert(bytes && bytes != caller_buffer && size <= RISC_TCP_BYTES_MAX);
    written.assign(static_cast<const char *>(bytes), size);
    *count = transfer_count; return transfer_result;
}
int32_t close(void *value, uint64_t handle) {
    entered(value); ++close_calls; closed.push_back(handle);
    return close_calls == fail_close_at ? close_result : RISC_TCP_OK;
}
risc_tcp_listener_v1 native_table{1, sizeof(native_table), context, listen, accept, read, write, close};
risc_provider_dependency_v1 dependency{RISC_TCP_LISTENER_CAPABILITY, 1, &native_table};
void start() {
    fake_driver = t5_driver_get(2);
    assert(fake_driver && !t5_driver_get(1) && !t5_driver_get(3));
    assert(fake_driver->abi_version == 2 && fake_driver->struct_size == sizeof(*fake_driver));
    assert(!std::strcmp(fake_driver->driver_id, "network-tcp-listener"));
    assert(!std::strcmp(fake_driver->capability_id, RISC_TCP_CONNECTION_CAPABILITY));
    assert(fake_driver->capability_api == 1 && fake_driver->quiesce);
    assert(fake_driver->start(&dependency, 1));
    connection = *static_cast<const risc_tcp_connection_v1 *>(fake_driver->capability);
    assert(connection.api_version == 1 && connection.struct_size == sizeof(connection) && connection.context);
    assert(!fake_driver->start(&dependency, 1) && calls == 0);
}
uint64_t listen() {
    risc_tcp_connection_listen_v1 request{sizeof(request), {192,168,7,6}, 8080, 0};
    caller_request = &request;
    uint64_t out = 99;
    assert(connection.listen(connection.context, &request, &out) == 0 && out);
    assert(request_copy.port == request.port && !std::memcmp(request_copy.address, request.address, 4));
    caller_request = nullptr;
    return out;
}
uint64_t accept(uint64_t listener) {
    uint64_t out = 99;
    assert(connection.accept(connection.context, listener, &out) == 0 && out > listener);
    return out;
}
void no_more_io(uint64_t listener, uint64_t client, int32_t expected) {
    const unsigned before = calls;
    uint64_t out = 99; uint32_t count = 99; char bytes[8]{};
    risc_tcp_connection_listen_v1 request{sizeof(request), {0,0,0,0}, 8080, 0};
    assert(connection.listen(connection.context, &request, &out) == expected && !out);
    out = 99;
    assert(connection.accept(connection.context, listener, &out) == expected && !out);
    assert(connection.read(connection.context, client, bytes, sizeof(bytes), &count) == expected && !count);
    count = 99;
    assert(connection.write(connection.context, client, bytes, sizeof(bytes), &count) == expected && !count);
    assert(connection.close(connection.context, client) == expected);
    assert(!fake_driver->quiesce()); fake_driver->stop();
    assert(!fake_driver->quiesce() && !fake_driver->start(&dependency, 1));
    assert(calls == before);
}
}

static void lifecycle() {
    using namespace Fake;
    Fake::start(); auto l = Fake::listen();
    uint64_t extra = 9; risc_tcp_connection_listen_v1 request{sizeof(request), {0,0,0,0}, 80, 0};
    assert(connection.listen(connection.context, &request, &extra) == RISC_TCP_CONNECTION_LIMIT && !extra);
    accept_result = RISC_TCP_WOULD_BLOCK;
    assert(connection.accept(connection.context, l, &extra) == RISC_TCP_CONNECTION_WOULD_BLOCK && !extra);
    accept_result = 0;
    auto c = Fake::accept(l); uint32_t n = 99; char bytes[8] = "abcdefg";
    caller_buffer = bytes; reenter = true;
    assert(connection.write(connection.context, c, bytes, 7, &n) == 0 && n == 3 && written == "abcdefg");
    assert(connection.read(connection.context, c, bytes, sizeof(bytes), &n) == 0 && n == 3);
    assert(!std::memcmp(bytes, "rrrdefg", 7)); reenter = false;
    char maximum[RISC_TCP_CONNECTION_BYTES_MAX]; std::memset(maximum, 'm', sizeof(maximum));
    caller_buffer = maximum; transfer_count = sizeof(maximum);
    assert(connection.write(connection.context, c, maximum, sizeof(maximum), &n) == 0 && n == sizeof(maximum));
    assert(written == std::string(sizeof(maximum), 'm'));
    assert(connection.read(connection.context, c, maximum, sizeof(maximum), &n) == 0 && n == sizeof(maximum));
    assert(std::all_of(maximum, maximum + sizeof(maximum), [](char x) { return x == 'r'; }));
    caller_buffer = bytes;
    transfer_result = RISC_TCP_WOULD_BLOCK; transfer_count = 0;
    assert(connection.read(connection.context, c, bytes, 8, &n) == RISC_TCP_CONNECTION_WOULD_BLOCK && !n);
    assert(connection.write(connection.context, c, bytes, 8, &n) == RISC_TCP_CONNECTION_WOULD_BLOCK && !n);
    transfer_result = RISC_TCP_EOF;
    assert(connection.read(connection.context, c, bytes, 8, &n) == RISC_TCP_CONNECTION_EOF && !n);
    transfer_result = RISC_TCP_NETWORK_DOWN;
    assert(connection.read(connection.context, c, bytes, 8, &n) == RISC_TCP_CONNECTION_NETWORK_DOWN && !n);
    assert(!std::memcmp(bytes, "rrrdefg", 8));
    const auto before = calls;
    assert(connection.close(connection.context, l) == RISC_TCP_CONNECTION_BUSY && calls == before);
    assert(connection.close(connection.context, c) == 0);
    assert(connection.close(connection.context, c) == RISC_TCP_CONNECTION_CONTEXT);
    assert(connection.read(connection.context, c, bytes, 8, &n) == RISC_TCP_CONNECTION_CONTEXT && !n);
    auto next = Fake::accept(l); assert(next > c);
    const auto native_client = native_generation, native_listener = uint64_t(1001);
    // The provider copied its dependency table; mutation does not redirect it.
    auto saved = native_table; native_table.context = nullptr; native_table.close = nullptr;
    assert(fake_driver->quiesce()); native_table = saved;
    assert(closed.size() == 3 && closed[1] == native_client && closed[2] == native_listener);
    auto old = connection;
    assert(fake_driver->quiesce()); fake_driver->stop();
    assert(connection.close(connection.context, l) == RISC_TCP_CONNECTION_CONTEXT);
    assert(fake_driver->start(&dependency, 1));
    connection = *static_cast<const risc_tcp_connection_v1 *>(fake_driver->capability);
    assert(connection.context != old.context);
    extra = 99;
    assert(old.listen(old.context, &request, &extra) == RISC_TCP_CONNECTION_CONTEXT && !extra);
    auto newer = Fake::listen(); assert(newer > next);
    assert(connection.close(connection.context, newer) == 0 && fake_driver->quiesce());
}
static void validation() {
    using namespace Fake;
    fake_driver = t5_driver_get(2);
    assert(fake_driver->quiesce()); fake_driver->stop();
    assert(!fake_driver->start(nullptr, 0) && !fake_driver->start(&dependency, 0) && !fake_driver->start(&dependency, 2));
    auto bad = dependency; bad.capability_id = "storage.app-data";
    assert(!fake_driver->start(&bad, 1)); bad = dependency; bad.api_version = 2;
    assert(!fake_driver->start(&bad, 1)); bad = dependency; bad.api = nullptr;
    assert(!fake_driver->start(&bad, 1));
    auto saved = native_table; native_table.struct_size = sizeof(native_table) - 1;
    assert(!fake_driver->start(&dependency, 1)); native_table = saved; native_table.api_version = 2;
    assert(!fake_driver->start(&dependency, 1)); native_table = saved; native_table.context = nullptr;
    assert(!fake_driver->start(&dependency, 1)); native_table = saved; native_table.close = nullptr;
    assert(!fake_driver->start(&dependency, 1)); native_table = saved;
    Fake::start(); uint64_t out = 99;
    risc_tcp_connection_listen_v1 r{sizeof(r), {0,0,0,0}, 8080, 0};
    assert(connection.listen(connection.context, nullptr, &out) == RISC_TCP_CONNECTION_INVALID && !out);
    r.port = 0; assert(connection.listen(connection.context, &r, &out) == RISC_TCP_CONNECTION_INVALID);
    r.port = 80; r.reserved = 1; assert(connection.listen(connection.context, &r, &out) == RISC_TCP_CONNECTION_INVALID);
    r.reserved = 0; r.struct_size--; assert(connection.listen(connection.context, &r, &out) == RISC_TCP_CONNECTION_INVALID);
    r.struct_size++; r.address[0] = 224; assert(connection.listen(connection.context, &r, &out) == RISC_TCP_CONNECTION_INVALID);
    r.address[0] = 0; assert(connection.listen(connection.context, &r, nullptr) == RISC_TCP_CONNECTION_INVALID);
    assert(!calls);
    auto l = Fake::listen(), c = Fake::accept(l); char bytes[8]{}; uint32_t n = 99;
    auto before = calls;
    assert(connection.accept(connection.context, c, &out) == RISC_TCP_CONNECTION_CONTEXT && !out);
    assert(connection.read(connection.context, l, bytes, sizeof(bytes), &n) == RISC_TCP_CONNECTION_CONTEXT && !n);
    assert(connection.read(nullptr, c, bytes, sizeof(bytes), &n) == RISC_TCP_CONNECTION_CONTEXT && !n);
    assert(connection.read(connection.context, c, nullptr, 1, &n) == RISC_TCP_CONNECTION_INVALID);
    assert(connection.read(connection.context, c, bytes, 0, &n) == RISC_TCP_CONNECTION_INVALID);
    assert(connection.read(connection.context, c, bytes, RISC_TCP_CONNECTION_BYTES_MAX + 1, &n) == RISC_TCP_CONNECTION_INVALID);
    assert(connection.read(connection.context, c, bytes, 8, nullptr) == RISC_TCP_CONNECTION_INVALID);
    assert(connection.close(connection.context, 0) == RISC_TCP_CONNECTION_CONTEXT && calls == before);
    for (unsigned i = 1; i < RISC_TCP_CONNECTION_CLIENTS_MAX; ++i) Fake::accept(l);
    before = calls;
    assert(connection.accept(connection.context, l, &out) == RISC_TCP_CONNECTION_LIMIT && !out && calls == before);
    assert(fake_driver->quiesce() && closed.size() == RISC_TCP_CONNECTION_CLIENTS_MAX + 1);
    assert(closed.back() == 1001);
}
static void failure(const std::string &test) {
    using namespace Fake;
    Fake::start(); uint64_t l = 0, c = 0; int32_t expected = RISC_TCP_CONNECTION_RETAINED;
    if (test == "listen-retained" || test == "listen-context" || test == "listen-error-handle" || test == "listen-zero") {
        listen_result = test == "listen-context" ? RISC_TCP_CONTEXT : RISC_TCP_RETAINED;
        if (test == "listen-context") expected = RISC_TCP_CONNECTION_CONTEXT;
        if (test == "listen-error-handle") { listen_result = RISC_TCP_IO; error_handle = 8; }
        if (test == "listen-zero") { listen_result = 0; zero_handle = true; }
        risc_tcp_connection_listen_v1 r{sizeof(r), {0,0,0,0}, 8080, 0};
        uint64_t out = 99;
        assert(connection.listen(connection.context, &r, &out) == expected && !out);
    } else {
        l = Fake::listen();
        if (test == "accept-retained" || test == "accept-zero" || test == "duplicate-native") {
            if (test == "accept-retained") accept_result = RISC_TCP_RETAINED;
            else if (test == "accept-zero") zero_handle = true;
            else duplicate_handle = native_generation;
            uint64_t out = 99;
            assert(connection.accept(connection.context, l, &out) == expected && !out);
        } else {
            c = Fake::accept(l);
            if (test == "close-retained" || test == "close-io" || test == "close-context") {
                close_result = test == "close-context" ? RISC_TCP_CONTEXT :
                    (test == "close-io" ? RISC_TCP_IO : RISC_TCP_RETAINED);
                if (test == "close-context") expected = RISC_TCP_CONNECTION_CONTEXT;
                assert(connection.close(connection.context, c) == expected && close_calls == 1);
            } else if (test == "quiesce-client" || test == "quiesce-listener") {
                close_result = RISC_TCP_RETAINED; fail_close_at = test == "quiesce-client" ? 1 : 2;
                assert(!fake_driver->quiesce() && close_calls == fail_close_at);
                assert(closed[0] == 1002);
                if (test == "quiesce-listener") assert(closed[1] == 1001);
            } else {
                char bytes[8] = "abcdefg"; uint32_t n = 99; caller_buffer = bytes;
                if (test == "read-context") { transfer_result = RISC_TCP_CONTEXT; expected = RISC_TCP_CONNECTION_CONTEXT; }
                else if (test == "read-retained") transfer_result = RISC_TCP_RETAINED;
                else if (test == "zero-success") transfer_count = 0;
                else if (test == "oversize-success") transfer_count = 9;
                else if (test == "wouldblock-bytes") transfer_result = RISC_TCP_WOULD_BLOCK;
                else if (test == "write-eof") { transfer_result = RISC_TCP_EOF; transfer_count = 0; }
                else if (test == "unknown-status") { transfer_result = 99; transfer_count = 0; }
                else assert(false);
                auto operation = test == "write-eof" ? connection.write : nullptr;
                auto result = operation ? operation(connection.context, c, bytes, sizeof(bytes), &n)
                                        : connection.read(connection.context, c, bytes, sizeof(bytes), &n);
                assert(result == expected && !n && !std::memcmp(bytes, "abcdefg", 8));
            }
        }
    }
    no_more_io(l, c, expected);
}
static void recoverable() {
    using namespace Fake;
    Fake::start(); risc_tcp_connection_listen_v1 r{sizeof(r), {0,0,0,0}, 8080, 0}; uint64_t out = 99;
    listen_result = RISC_TCP_NETWORK_DOWN;
    assert(connection.listen(connection.context, &r, &out) == RISC_TCP_CONNECTION_NETWORK_DOWN && !out);
    listen_result = RISC_TCP_IO;
    assert(connection.listen(connection.context, &r, &out) == RISC_TCP_CONNECTION_IO && !out);
    listen_result = 0; auto l = Fake::listen();
    accept_result = RISC_TCP_IO;
    assert(connection.accept(connection.context, l, &out) == RISC_TCP_CONNECTION_IO && !out);
    accept_result = 0; auto c = Fake::accept(l); char bytes[8]{}; uint32_t n = 99;
    transfer_result = RISC_TCP_IO; transfer_count = 0; caller_buffer = bytes;
    assert(connection.write(connection.context, c, bytes, 8, &n) == RISC_TCP_CONNECTION_IO && !n);
    transfer_result = RISC_TCP_OK; transfer_count = 3;
    assert(connection.write(connection.context, c, bytes, 8, &n) == 0 && n == 3);
    assert(fake_driver->quiesce());
}
static void overflow() {
    using namespace Fake;
    Fake::start(); auto l = Fake::listen();
    ::session_generation = UINT64_MAX - 1;
    const auto c = Fake::accept(l); assert(c == UINT64_MAX);
    uint64_t out = 99; const auto before = calls;
    assert(connection.accept(connection.context, l, &out) == RISC_TCP_CONNECTION_LIMIT && !out && calls == before);
    assert(connection.close(connection.context, c) == 0 && connection.close(connection.context, l) == 0);
    risc_tcp_connection_listen_v1 request{sizeof(request), {0,0,0,0}, 8080, 0};
    assert(connection.listen(connection.context, &request, &out) == RISC_TCP_CONNECTION_LIMIT && !out);
    assert(fake_driver->quiesce());
    ::activation_generation = UINTPTR_MAX - 1;
    assert(fake_driver->start(&dependency, 1));
    connection = *static_cast<const risc_tcp_connection_v1 *>(fake_driver->capability);
    assert(reinterpret_cast<uintptr_t>(connection.context) == UINTPTR_MAX && fake_driver->quiesce());
    assert(!fake_driver->start(&dependency, 1));
}
int main(int argc, char **argv) {
    assert(argc == 2); const std::string test = argv[1];
    if (test == "lifecycle") lifecycle();
    else if (test == "validation") validation();
    else if (test == "recoverable") recoverable();
    else if (test == "overflow") overflow();
    else failure(test);
    std::printf("TCP listener service: %s passed\n", test.c_str());
}

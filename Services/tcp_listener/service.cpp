/* Ordinary capability facade. The runtime-selected dependency owns all socket
 * and owner-task authority; this provider adds bounded copies and local custody.
 * Calls are serialized by the provider owner. The busy guard rejects reentry. */
#include "RiscProviderV2.h"
#include "RiscTcpConnectionV1.h"
#include "native/RiscTcpListenerV1.h"
#include <cstring>

namespace {
static_assert(RISC_TCP_CONNECTION_BYTES_MAX == RISC_TCP_BYTES_MAX, "transfer bound");
static_assert(RISC_TCP_CONNECTION_CLIENTS_MAX == RISC_TCP_CLIENTS_MAX, "client bound");
static_assert(int(RISC_TCP_CONNECTION_RETAINED) == int(RISC_TCP_RETAINED), "status ABI");
struct Session { uint64_t handle, native; };
risc_tcp_listener_v1 backend{};
Session listener{}, clients[RISC_TCP_CONNECTION_CLIENTS_MAX]{};
uint8_t buffer[RISC_TCP_CONNECTION_BYTES_MAX];
uintptr_t activation_generation;
uint64_t session_generation;
int32_t terminal;
bool started, busy, stopping;
extern risc_tcp_connection_v1 api;

struct Operation {
    Operation() { busy = true; }
    ~Operation() { busy = false; }
};
int32_t fence(int32_t error) {
    terminal = error == RISC_TCP_CONTEXT ? RISC_TCP_CONNECTION_CONTEXT : RISC_TCP_CONNECTION_RETAINED;
    return terminal;
}
int32_t gate(void *context) {
    if (!started || !context || context != api.context) return RISC_TCP_CONNECTION_CONTEXT;
    if (terminal) return terminal;
    if (stopping) return RISC_TCP_CONNECTION_CONTEXT;
    return busy ? RISC_TCP_CONNECTION_BUSY : RISC_TCP_CONNECTION_OK;
}
bool has_clients() {
    for (const auto &client : clients) if (client.handle) return true;
    return false;
}
Session *find_client(uint64_t handle) {
    if (!handle) return nullptr;
    for (auto &client : clients) if (client.handle == handle) return &client;
    return nullptr;
}
bool native_held(uint64_t handle) {
    if (listener.native == handle) return true;
    for (const auto &client : clients) if (client.native == handle) return true;
    return false;
}
bool known_error(int32_t result) {
    return result >= RISC_TCP_BUSY && result <= RISC_TCP_INVALID;
}
int32_t opened(Session &slot, int32_t result, uint64_t native, uint64_t *out, bool accepting) {
    if (result == RISC_TCP_CONTEXT || result == RISC_TCP_RETAINED) return fence(result);
    if (result != RISC_TCP_OK) {
        // A handle reported with an error has ambiguous custody; never guess.
        if (native || (!known_error(result) && !(accepting && result == RISC_TCP_WOULD_BLOCK)))
            return fence(RISC_TCP_RETAINED);
        return result;
    }
    if (!native || native_held(native)) return fence(RISC_TCP_RETAINED);
    slot = Session{++session_generation, native};
    *out = slot.handle;
    return RISC_TCP_CONNECTION_OK;
}
int32_t listen(void *context, const risc_tcp_connection_listen_v1 *request, uint64_t *out) {
    if (out) *out = 0;
    int32_t result = gate(context); if (result) return result;
    if (!request || !out || request->struct_size != sizeof(*request) ||
        !request->port || request->reserved || request->address[0] >= 224)
        return RISC_TCP_CONNECTION_INVALID;
    if (listener.handle || has_clients() || session_generation == UINT64_MAX)
        return RISC_TCP_CONNECTION_LIMIT;
    // Copy the request; no caller memory reaches the native dependency.
    risc_tcp_listen_v1 copied{};
    copied.struct_size = sizeof(copied);
    std::memcpy(copied.address, request->address, sizeof(copied.address));
    copied.port = request->port;
    Operation operation;
    uint64_t native = 0;
    result = backend.listen(backend.context, &copied, &native);
    return opened(listener, result, native, out, false);
}
int32_t accept(void *context, uint64_t handle, uint64_t *out) {
    if (out) *out = 0;
    int32_t result = gate(context); if (result) return result;
    if (!out) return RISC_TCP_CONNECTION_INVALID;
    if (!handle || handle != listener.handle) return RISC_TCP_CONNECTION_CONTEXT;
    Session *slot = nullptr;
    for (auto &client : clients) if (!client.handle) { slot = &client; break; }
    if (!slot || session_generation == UINT64_MAX) return RISC_TCP_CONNECTION_LIMIT;
    Operation operation;
    uint64_t native = 0;
    result = backend.accept(backend.context, listener.native, &native);
    return opened(*slot, result, native, out, true);
}
int32_t transfer(void *context, uint64_t handle, void *output, const void *input,
                 uint32_t size, uint32_t *count) {
    if (count) *count = 0;
    int32_t result = gate(context); if (result) return result;
    Session *client = find_client(handle);
    if (!client) return RISC_TCP_CONNECTION_CONTEXT;
    if (!count || (!output && !input) || !size || size > sizeof(buffer))
        return RISC_TCP_CONNECTION_INVALID;
    Operation operation;
    if (input) std::memcpy(buffer, input, size);
    uint32_t transferred = 0;
    result = output ? backend.read(backend.context, client->native, buffer, size, &transferred)
                    : backend.write(backend.context, client->native, buffer, size, &transferred);
    if (result == RISC_TCP_CONTEXT || result == RISC_TCP_RETAINED) result = fence(result);
    else if (result == RISC_TCP_OK) {
        if (!transferred || transferred > size) result = fence(RISC_TCP_RETAINED);
        else {
            if (output) std::memcpy(output, buffer, transferred);
            *count = transferred;
        }
    } else if (transferred || (!known_error(result) && result != RISC_TCP_WOULD_BLOCK &&
                               !(output && result == RISC_TCP_EOF))) result = fence(RISC_TCP_RETAINED);
    std::memset(buffer, 0, sizeof(buffer));
    return result;
}
int32_t read(void *context, uint64_t handle, void *bytes, uint32_t size, uint32_t *count) {
    return transfer(context, handle, bytes, nullptr, size, count);
}
int32_t write(void *context, uint64_t handle, const void *bytes, uint32_t size, uint32_t *count) {
    return transfer(context, handle, nullptr, bytes, size, count);
}
int32_t close_session(Session &session) {
    const int32_t result = backend.close(backend.context, session.native);
    // Even IO/INVALID/BUSY here cannot establish that a close did not happen.
    // Latch custody and never repeat an uncertain native close.
    if (result != RISC_TCP_OK) return fence(result);
    session = Session{};
    return RISC_TCP_CONNECTION_OK;
}
int32_t close(void *context, uint64_t handle) {
    const int32_t result = gate(context); if (result) return result;
    Session *session = find_client(handle);
    if (!session) {
        if (!handle || listener.handle != handle) return RISC_TCP_CONNECTION_CONTEXT;
        if (has_clients()) return RISC_TCP_CONNECTION_BUSY;
        session = &listener;
    }
    Operation operation;
    return close_session(*session);
}
bool quiesce() {
    if (busy || terminal) return false;
    if (!started) return true;
    stopping = true;
    Operation operation;
    for (auto &client : clients)
        if (client.handle && close_session(client) != RISC_TCP_CONNECTION_OK) return false;
    if (listener.handle && close_session(listener) != RISC_TCP_CONNECTION_OK) return false;
    std::memset(buffer, 0, sizeof(buffer));
    backend = risc_tcp_listener_v1{};
    api.context = nullptr;
    started = false;
    return true;
}
bool start(const risc_provider_dependency_v1 *dependencies, size_t count) {
    if (started || busy || terminal || listener.handle || has_clients() ||
        activation_generation == UINTPTR_MAX || !dependencies || count != 1) return false;
    const auto &dependency = dependencies[0];
    if (!dependency.capability_id || std::strcmp(dependency.capability_id, RISC_TCP_LISTENER_CAPABILITY) ||
        dependency.api_version != RISC_TCP_LISTENER_API_V1 || !dependency.api) return false;
    const auto *candidate = static_cast<const risc_tcp_listener_v1 *>(dependency.api);
    if (candidate->api_version != RISC_TCP_LISTENER_API_V1 || candidate->struct_size < sizeof(*candidate) ||
        !candidate->context || !candidate->listen || !candidate->accept || !candidate->read ||
        !candidate->write || !candidate->close) return false;
    backend = *candidate;
    api.context = reinterpret_cast<void *>(++activation_generation);
    stopping = false;
    started = true;
    return true;
}
void stop() { (void)quiesce(); }
risc_tcp_connection_v1 api{RISC_TCP_CONNECTION_API_V1, sizeof(api), nullptr, listen, accept, read, write, close};
const risc_driver_v2 driver{RISC_PROVIDER_DRIVER_ABI_V2, sizeof(driver), "network-tcp-listener",
    RISC_TCP_CONNECTION_CAPABILITY, RISC_TCP_CONNECTION_API_V1, &api, start, stop, quiesce};
}
extern "C" __attribute__((visibility("default"))) const risc_driver_v2 *t5_driver_get(uint32_t abi) {
    return abi == RISC_PROVIDER_DRIVER_ABI_V2 ? &driver : nullptr;
}

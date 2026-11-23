#include <cstdio>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <Arduino.h>

static uint64_t clockMs = 0, caseStart = 0;
static uint32_t guardMs = 2500;
uint32_t millis() { return static_cast<uint32_t>(clockMs); }
void delay(uint32_t milliseconds) {
    clockMs += milliseconds;
    if (clockMs - caseStart > guardMs)
        throw std::runtime_error("driver exceeded deterministic safety guard");
}
static void resetClock(uint64_t start = 0) { clockMs = caseStart = start; guardMs = 2500; }
static void require(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}

// Actual production dependency sources, including the private poll function.
// No copied/reimplemented driver routine is substituted in this translation unit.
#include "SparkFun_VL53L5CX_IO.cpp"
#include "platform.cpp"
#include "vl53l5cx_api.cpp"
#include "SparkFun_VL53L5CX_Library.cpp"

struct Head {
    SparkFun_VL53L5CX_IO io;
    VL53L5CX_Configuration dev{};
    SparkFun_VL53L5CX driver;
    Head(TwoWire &wire, uint8_t address) {
        require(io.begin(address, wire), "fixture bus must initially ACK");
        io.setMaxPacketSize(128);
        dev.platform.VL53L5CX_i2c = &io;
        driver.Dev = &dev; driver.VL53L5CX_i2c = &io;
    }
};
static std::vector<uint8_t> rangeFrame() {
    std::vector<uint8_t> result(16, 0);
    auto append = [&](uint32_t header, const void *data, size_t size) {
        const size_t start = result.size(); result.resize(start + 4 + size);
        std::memcpy(result.data() + start, &header, 4);
        std::memcpy(result.data() + start + 4, data, size);
    };
    int16_t raw[64]; uint8_t status[64], detected[64];
    for (int i = 0; i < 64; ++i) { raw[i] = 4000; status[i] = 5; detected[i] = 1; }
    append(VL53L5CX_DISTANCE_BH, raw, sizeof raw);
    append(VL53L5CX_TARGET_STATUS_BH, status, sizeof status);
    append(VL53L5CX_NB_TARGET_DETECTED_BH, detected, sizeof detected);
    SwapBuffer(result.data(), static_cast<uint16_t>(result.size()));
    result[0] = 17;
    return result;
}
static void zeroRead() {
    TwoWire wire; Head head(wire, 0x2a); wire.peers[0x2a].count = 0;
    uint8_t output[8]; std::memset(output, 0xa5, sizeof output);
    require(head.io.readMultipleBytes(0, output, sizeof output) != 0, "zero-byte read reported success");
    for (uint8_t value : output) require(value == 0xa5, "missing read changed output");
    require(wire.peers[0x2a].reads == 0, "read called without received bytes");
}
static void shortRead() {
    TwoWire wire; Head head(wire, 0x2a); auto &peer = wire.peers[0x2a];
    peer.count = 2; peer.value = 0x5a;
    uint8_t output[8]; std::memset(output, 0xa5, sizeof output);
    require(head.io.readMultipleBytes(0, output, sizeof output) != 0, "short read reported success");
    require(peer.reads <= 2, "short read fabricated missing bytes");
}
static void byteError() {
    TwoWire wire; Head head(wire, 0x2a); wire.peers[0x2a].count = 0;
    uint8_t output = 0xa5;
    require(RdByte(&head.dev.platform, 0, &output) != 0, "RdByte swallowed missing-byte error");
    require(output == 0xa5, "failed RdByte overwrote output");
}
static void chunkedSuccess() {
    TwoWire wire; Head head(wire, 0x2a);
    auto &memory = wire.peers[0x2a].memory;
    for (unsigned i = 0; i < 260; ++i) memory.push_back(static_cast<uint8_t>(i));
    std::vector<uint8_t> output(memory.size());
    require(head.io.readMultipleBytes(0, output.data(), output.size()) == 0, "complete chunked read failed");
    require(output == memory, "chunked read corrupted data");
    require(wire.requested == std::vector<size_t>({128, 128, 4}), "chunk boundaries changed");
}
static void transactionError() {
    TwoWire wire; Head head(wire, 0x2a); wire.peers[0x2a].nack = 2;
    uint8_t output = 0;
    require(head.io.readMultipleBytes(0, &output, 1) == 2, "register-address error lost");
    require(wire.peers[0x2a].requests == 0, "read followed failed register address");
}
static void pollTimeout() {
    resetClock(); TwoWire wire; Head head(wire, 0x2a);
    require(_vl53l5cx_poll_for_answer(&head.dev, 1, 0, 6, 255, 1) != 0, "nonmatching poll succeeded");
    require(clockMs - caseStart <= 2010, "nominal 2s poll did not terminate");
}
static void pollWrapAndDelayedSuccess() {
    resetClock(0xffffff00ULL); TwoWire wire; Head head(wire, 0x2a);
    require(_vl53l5cx_poll_for_answer(&head.dev, 1, 0, 6, 255, 1) != 0, "wrap timeout succeeded");
    require(clockMs - caseStart <= 2010, "wrap timeout was not bounded");
    resetClock(0xffffff00ULL);
    wire.peers[0x2a].response = [](uint16_t) { return clockMs - caseStart >= 1500 ? 1 : 0; };
    require(_vl53l5cx_poll_for_answer(&head.dev, 1, 0, 6, 255, 1) == 0, "delayed valid answer rejected");
    require(clockMs - caseStart >= 1500 && clockMs - caseStart <= 1520, "delayed answer timing wrong");
}
static void pollErrors() {
    resetClock(); TwoWire wire; Head head(wire, 0x2a); auto &peer = wire.peers[0x2a];
    peer.count = 0;
    require(_vl53l5cx_poll_for_answer(&head.dev, 1, 0, 6, 255, 1) != 0, "poll swallowed transport error");
    require(clockMs == caseStart && peer.requests == 1, "poll retried transport error");
    resetClock(); peer.count = -1; peer.value = 0x7f;
    require(_vl53l5cx_poll_for_answer(&head.dev, 4, 1, 6, 255, 3) == VL53L5CX_MCU_ERROR, "MCU error lost");
    require(clockMs - caseStart <= 10, "MCU error did not stop polling");
}
static void initTimeout() {
    resetClock(); TwoWire wire; Head head(wire, 0x2a);
    require(vl53l5cx_init(&head.dev) != 0, "failed initialization reported success");
    require(clockMs - caseStart <= 2110, "init continued beyond failed boot stage");
    require(wire.peers[0x2a].bulkWrites == 0, "firmware download followed failed boot poll");
}
static void initTransportError() {
    resetClock(); TwoWire wire; Head head(wire, 0x2a); auto &peer = wire.peers[0x2a];
    const unsigned before = peer.transmissions; peer.nack = 2;
    require(vl53l5cx_init(&head.dev) == 2, "init lost first transport error");
    require(peer.transmissions == before + 1 && clockMs == caseStart, "init kept issuing failed operations");
}
static void configurationErrors() {
    using Operation = std::function<uint8_t(VL53L5CX_Configuration *)>;
    const std::vector<Operation> operations = {
        [](VL53L5CX_Configuration *d) { return vl53l5cx_set_resolution(d, 64); },
        [](VL53L5CX_Configuration *d) { return vl53l5cx_set_ranging_frequency_hz(d, 5); },
        [](VL53L5CX_Configuration *d) { return vl53l5cx_start_ranging(d); }
    };
    for (const auto &operation : operations) {
        resetClock(); TwoWire wire; Head head(wire, 0x2a); auto &peer = wire.peers[0x2a];
        const unsigned before = peer.transmissions; peer.nack = 2;
        require(operation(&head.dev) == 2, "configuration/start lost transport error");
        require(peer.transmissions == before + 1 && clockMs == caseStart, "configuration continued after first failure");
    }
}
static void readinessReadError() {
    TwoWire wire; Head head(wire, 0x2a); auto &peer = wire.peers[0x2a];
    head.dev.streamcount = 41; peer.memory = {42, 5, 5, 0x10};
    uint8_t ready = 0;
    require(vl53l5cx_check_data_ready(&head.dev, &ready) == 0 && ready == 1, "valid readiness not preserved");
    head.dev.streamcount = 41; peer.count = 0; ready = 1;
    require(vl53l5cx_check_data_ready(&head.dev, &ready) != 0, "readiness swallowed I2C error");
    require(ready == 0 && head.dev.streamcount == 41, "failed readiness consumed stale stream identity");
}
static void rangeReadError() {
    TwoWire wire; Head head(wire, 0x2a);
    head.dev.streamcount = 41; head.dev.data_read_size = 32;
    wire.peers[0x2a].count = 0;
    VL53L5CX_ResultsData result; std::memset(&result, 0x5a, sizeof result);
    const VL53L5CX_ResultsData before = result;
    require(!head.driver.getRangingData(&result), "range read swallowed I2C error");
    require(std::memcmp(&result, &before, sizeof result) == 0, "failed range read parsed/converted stale output");
    require(head.dev.streamcount == 41, "failed range read changed stream identity");
}
static void rangeSuccessAndPartialFailure() {
    TwoWire wire; Head head(wire, 0x2b); auto &peer = wire.peers[0x2b];
    peer.memory = rangeFrame(); head.dev.data_read_size = peer.memory.size();
    VL53L5CX_ResultsData result{};
    require(head.driver.getRangingData(&result), "valid range frame rejected");
    for (unsigned i = 0; i < 64; ++i)
        require(result.distance_mm[i] == 1000 && result.target_status[i] == 5, "valid range conversion changed");
    const VL53L5CX_ResultsData before = result; const uint8_t stream = head.dev.streamcount;
    peer.failAt = peer.requests + 2;
    require(!head.driver.getRangingData(&result), "partial multi-chunk range read accepted");
    require(std::memcmp(&result, &before, sizeof result) == 0 && head.dev.streamcount == stream,
            "partial range read changed published values or identity");
}
static void peerAfterRecoveryFailure() {
    TwoWire wire; Head first(wire, 0x2a), second(wire, 0x2b);
    wire.peers[0x2b].memory = rangeFrame();
    second.dev.data_read_size = wire.peers[0x2b].memory.size();
    for (unsigned attempt = 0; attempt < 2; ++attempt) {
        resetClock();
        require(vl53l5cx_init(&first.dev) != 0, "faulted first head initialized");
        require(clockMs - caseStart <= 2110, "first head blocked shared-worker progress");
        VL53L5CX_ResultsData result{};
        require(second.driver.getRangingData(&result), "healthy second head not serviceable after failed init");
        require(result.distance_mm[0] == 1000 && result.target_status[0] == 5, "second head data changed");
    }
}
int main(int argc, char **argv) {
    const std::vector<std::pair<const char *, std::function<void()>>> cases = {
        {"zero-read", zeroRead}, {"short-read", shortRead}, {"byte-error", byteError},
        {"chunked-success", chunkedSuccess}, {"transaction-error", transactionError},
        {"poll-timeout", pollTimeout}, {"poll-wrap-delayed", pollWrapAndDelayedSuccess},
        {"poll-errors", pollErrors}, {"init-timeout", initTimeout},
        {"init-transport-error", initTransportError}, {"configuration-errors", configurationErrors},
        {"readiness-error", readinessReadError}, {"range-read-error", rangeReadError},
        {"range-success-partial", rangeSuccessAndPartialFailure}, {"peer-after-recovery", peerAfterRecoveryFailure}
    };
    unsigned ran = 0;
    for (const auto &test : cases) {
        if (argc > 1 && std::string(argv[1]) != test.first) continue;
        ++ran;
        try { resetClock(); test.second(); std::printf("PASS %s\n", test.first); }
        catch (const std::exception &error) {
            std::printf("FAIL %s: %s\n", test.first, error.what()); return 1;
        }
        std::fflush(stdout);
    }
    if (!ran) { std::fprintf(stderr, "Unknown test case\n"); return 2; }
    std::printf("PASS %u production LiDAR driver groups\n", ran);
    return 0;
}

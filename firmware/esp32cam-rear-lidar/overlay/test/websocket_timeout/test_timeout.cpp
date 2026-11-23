#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <stdexcept>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <sys/time.h>

using String = std::string;
static unsigned long clockMs = 100;
unsigned long millis() { return clockMs; }
#define DEBUG_WEBSOCKETS(...) do {} while (0)
#define WEBSOCKETS_YIELD() do {} while (0)
#define ESP32 1
#define WEBSOCKETS_NETWORK_TYPE NETWORK_ESP32
#include "actual_constants.inc"

class Stream {
protected:
    unsigned long _timeout = 1000, _startMillis = 0;
    int timedRead();
public:
    virtual ~Stream() {}
    virtual int available() = 0;
    virtual int read() = 0;
    void setTimeout(unsigned long);
    unsigned long getTimeout();
    virtual size_t readBytes(char *, size_t);
    String readStringUntil(char);
};
class Client : public Stream {};

struct Arrival { unsigned long at; String bytes; };
class WiFiClient : public Client {
public:
    explicit WiFiClient(String initial) : input(initial) {}
    int setTimeout(uint32_t seconds);
    int fd() const { return 7; }
    int setSocketOption(int option, char *data, size_t size) {
        if (size != sizeof(timeval)) throw std::runtime_error("wrong socket boundary size");
        const timeval &value = *reinterpret_cast<timeval *>(data);
        if (option == SO_RCVTIMEO) receiveSeconds = value.tv_sec;
        else if (option == SO_SNDTIMEO) sendSeconds = value.tv_sec;
        else throw std::runtime_error("unexpected socket option");
        return 0;
    }
    void setNoDelay(bool value) { noDelay = value; }
    int available() override { release(); return static_cast<int>(input.size() - position); }
    int read() override {
        release();
        if (position < input.size()) return static_cast<unsigned char>(input[position++]);
        // The network/clock boundary advances deterministic milliseconds.
        // Production timedRead/readStringUntil themselves are unmodified.
        ++clockMs;
        return -1;
    }
    void later(unsigned long after, const String &bytes) {
        arrivals.push_back({clockMs + after, bytes});
    }
    long receiveSeconds = -1, sendSeconds = -1;
    bool noDelay = false;
    unsigned long connectionTimeout() const { return _timeout; }
private:
    unsigned long _timeout = 3000; // WiFiClient's separate connection timeout.
    String input;
    size_t position = 0;
    std::deque<Arrival> arrivals;
    void release() {
        while (!arrivals.empty() && arrivals.front().at <= clockMs) {
            input += arrivals.front().bytes;
            arrivals.pop_front();
        }
    }
};

enum WSclientsStatus_t { WSC_NOT_CONNECTED, WSC_HEADER, WSC_BODY, WSC_CONNECTED };
struct WSclient_t { WiFiClient *tcp = nullptr; WSclientsStatus_t status = WSC_NOT_CONNECTED; };
class WebSockets {
protected:
    void handleWebsocket(WSclient_t *) { throw std::runtime_error("not a control-parser test"); }
    void clientDisconnect(WSclient_t *client, int) { client->status = WSC_NOT_CONNECTED; }
};
class WebSocketsClient : public WebSockets {
public:
    explicit WebSocketsClient(WiFiClient &tcp) { _client.tcp = &tcp; }
    void connectedCb();
    void handleClientData();
    bool disconnected = false;
    unsigned headersSent = 0;
    std::vector<String> receivedLines;
private:
    WSclient_t _client;
    unsigned long _lastHeaderSent = 0;
    void sendHeader(WSclient_t *) { ++headersSent; _lastHeaderSent = millis(); }
    void handleHeader(WSclient_t *, String *line) { receivedLines.push_back(*line); }
    void clientDisconnect(WSclient_t *client, const char *) {
        disconnected = true; client->status = WSC_NOT_CONNECTED;
    }
};

// Verbatim methods extracted from checksum-verified WebSockets 2.7.2 and
// Arduino ESP32 2.0.17 source. The old/new test differs only in connectedCb.
#include "actual_methods.inc"

static void require(bool result, const char *why) {
    if (!result) throw std::runtime_error(why);
}

static void partialHeaderUsesIntendedIdleDeadline() {
    clockMs = 100;
    WiFiClient tcp("HTTP/1.1 101"); WebSocketsClient socket(tcp);
    socket.connectedCb();
    const unsigned long start = clockMs;
    socket.handleClientData();
    const unsigned long elapsed = clockMs - start;
    std::printf("partial_header_elapsed_ms=%lu stream_timeout_ms=%lu socket_timeout_seconds=%ld\n",
                elapsed, tcp.getTimeout(), tcp.receiveSeconds);
    require(elapsed == 5000, "partial header exceeded intended 5000 ms idle timeout");
    require(tcp.getTimeout() == 5000 && tcp.connectionTimeout() == 5000,
            "Stream and connection timeouts must use their own units");
    require(tcp.receiveSeconds == 5 && tcp.sendSeconds == 5, "socket timeouts must be seconds");
    require(socket.receivedLines.size() == 1 && socket.receivedLines[0] == "HTTP/1.1 101",
            "timed read changed the received prefix");
    ++clockMs;
    socket.handleClientData();
    require(socket.disconnected, "existing outer header timeout must still close an incomplete handshake");
}

static void completeHeaderDoesNotWait() {
    clockMs = 100;
    WiFiClient tcp("HTTP/1.1 101 Switching Protocols\r\n"); WebSocketsClient socket(tcp);
    socket.connectedCb();
    socket.handleClientData();
    require(clockMs == 100 && !socket.disconnected, "complete header should not wait");
    require(socket.headersSent == 1 && tcp.noDelay, "handshake setup behavior changed");
    require(socket.receivedLines.size() == 1 &&
            socket.receivedLines[0] == "HTTP/1.1 101 Switching Protocols\r",
            "complete header content changed");
}

static void fragmentedHeaderBeforeDeadlineCompletes() {
    clockMs = 100;
    WiFiClient tcp("HTTP/1.1 "); tcp.later(4999, "101 Switching Protocols\r\n");
    WebSocketsClient socket(tcp); socket.connectedCb(); socket.handleClientData();
    require(clockMs == 5099 && !socket.disconnected, "in-budget partial header was dropped");
    require(socket.receivedLines.size() == 1 &&
            socket.receivedLines[0] == "HTTP/1.1 101 Switching Protocols\r",
            "fragmented header content changed");
}

static void ByteProgressKeepsExistingIdleSemantics() {
    clockMs = 100;
    WiFiClient tcp("H"); tcp.later(4000, "T"); tcp.later(8000, "TP\r\n");
    WebSocketsClient socket(tcp); socket.connectedCb(); socket.handleClientData();
    require(clockMs == 8100 && socket.receivedLines.size() == 1 &&
            socket.receivedLines[0] == "HTTP\r", "byte progress must retain the existing idle semantics");
}

int main(int argc, char **argv) {
    try {
        if (argc == 2 && !std::strcmp(argv[1], "partial-header")) {
            partialHeaderUsesIntendedIdleDeadline();
            return 0;
        }
        partialHeaderUsesIntendedIdleDeadline(); puts("PASS partial HTTP header 5000 ms idle bound");
        completeHeaderDoesNotWait(); puts("PASS complete handshake line without delay");
        fragmentedHeaderBeforeDeadlineCompletes(); puts("PASS fragmented line inside original idle budget");
        ByteProgressKeepsExistingIdleSemantics(); puts("PASS byte progress preserves idle rather than total deadline");
        puts("4 actual-method timeout groups passed");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}

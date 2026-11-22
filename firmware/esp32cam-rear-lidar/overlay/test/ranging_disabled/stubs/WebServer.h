#pragma once
#include <functional>
#include <map>
#include <string>
#include <utility>
enum HTTPMethod { HTTP_GET, HTTP_POST };
class WebServer {
public:
    std::map<std::pair<std::string, HTTPMethod>, std::function<void()>> handlers;
    int responseCode = 0;
    std::string responseBody;
    void on(const char *path, HTTPMethod method, std::function<void()> callback) {
        handlers[{path, method}] = callback;
    }
    void sendHeader(const char *, const char *) {}
    void send(int code, const char *, const char *body) {
        responseCode = code;
        responseBody = body;
    }
    void request(const char *path, HTTPMethod method) {
        responseCode = 0;
        responseBody.clear();
        handlers.at({path, method})();
    }
};

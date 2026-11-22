#include <cassert>
#include <iostream>
#include "Arduino.h"
#include "WiFi.h"
#include "esp_wifi.h"
#include "freertos/task.h"
#include "../../src/rear_ranging.cpp"

TestEsp ESP;
TestWiFi WiFi;
int testWifiStatus = WL_CONNECTED;
int testPowerReads = 0, testPowerWrites = 0;
int8_t testPower = 78;
std::vector<TestTask> testTasks;
WebServer server;
bool allowRequest = true;
bool authorize() {
    if (!allowRequest) server.send(403, "application/json", "{\"error\":\"unauthorized\"}");
    return allowRequest;
}

void assertDisabled(JsonObjectConst output) {
    assert(!output["ranging_available"].as<bool>());
    assert(!output["ranging_enabled"].as<bool>());
    assert(output["ranging_disabled_reason"] == "disabled_in_firmware");
    for (const char *key : {"ftm", "ftm_initiator", "ftm_responder"})
        assert(output["capabilities"][key].is<bool>() && !output["capabilities"][key].as<bool>());
    const auto ftm = output["ftm"];
    assert(!ftm["supported"].as<bool>() && !ftm["enabled"].as<bool>());
    assert(!ftm["busy"].as<bool>() && !ftm["retiring"].as<bool>());
    assert(ftm["status"] == "disabled_in_firmware");
    assert(ftm["report_id"].as<unsigned>() == 0);
    assert(ftm["samples"].as<unsigned>() == 0);
    for (const char *key : {"distance_m", "rtt_ns", "age_ms", "target", "channel"})
        assert(ftm[key].isNull());
    const auto responder = output["ftm_responder"];
    assert(!responder["supported"].as<bool>() && !responder["enabled"].as<bool>() &&
           !responder["active"].as<bool>());
    assert(responder["status"] == "disabled_in_firmware");
    assert(responder["ssid"].isNull() && responder["bssid"].isNull());
    assert(responder["firmware"] == APP_VERSION);
}

int main() {
    rear_ranging::begin(server, authorize);
    assert(testTasks.size() == 1 && testTasks[0].name == "rear-txpower");
    assert(testTasks[0].priority == 1);
    rear_ranging::begin(server, authorize);
    assert(testTasks.size() == 1); // repeated setup cannot create another worker
    for (const char *path : {"/api/telemetry/ftm", "/api/telemetry/config"}) {
        server.request(path, HTTP_POST);
        assert(server.responseCode == 501);
        DynamicJsonDocument reply(512);
        assert(!deserializeJson(reply, server.responseBody));
        assert(reply["reason"] == "disabled_in_firmware");
        assert(reply["firmware"] == APP_VERSION);
    }
    std::cout << "PASS outgoing FTM and enable-config writes rejected\n";

    for (const char *path : {"/api/ranging", "/api/telemetry/config"}) {
        server.request(path, HTTP_GET);
        assert(server.responseCode == 200);
        DynamicJsonDocument reply(4096);
        assert(!deserializeJson(reply, server.responseBody));
        assertDisabled(reply.as<JsonObjectConst>());
        assert(!reply["config"]["ftm_responder_enabled"].as<bool>());
        assert(!reply["config"]["ftm_initiator_enabled"].as<bool>());
        assert(!reply["config"]["mutable"].as<bool>());
    }
    std::cout << "PASS disabled capabilities and immutable configuration\n";

    DynamicJsonDocument packet(4096);
    packet["ftm"]["distance_m"] = 12.3;
    packet["ftm"]["rtt_ns"] = 82;
    packet["ftm"]["samples"] = 16;
    packet["ftm_responder"]["active"] = true;
    rear_ranging::writeJson(packet.as<JsonObject>(), 1);
    assertDisabled(packet.as<JsonObjectConst>());
    assert(packet["tx_power_actual_dbm"].isNull());
    std::cout << "PASS no retained valid range or invented zero distance\n";

    rear_ranging::updateTxPower();
    assert(testPower == 60 && testPowerWrites == 1);
    rear_ranging::updateTxPower();
    assert(testPowerWrites == 1);
    rear_ranging::writeJson(packet.as<JsonObject>(), 2);
    assert(packet["tx_power_requested_dbm"].as<double>() == 15);
    assert(packet["tx_power_actual_dbm"].as<double>() == 15);
    const int reads = testPowerReads;
    rear_ranging::pause(true);
    rear_ranging::updateTxPower();
    assert(testPowerReads == reads);
    rear_ranging::pause(false);
    testWifiStatus = 0;
    rear_ranging::updateTxPower();
    assert(testPowerReads == reads);
    std::cout << "PASS existing station power limit and OTA pause\n";

    allowRequest = false;
    server.request("/api/telemetry/ftm", HTTP_POST);
    assert(server.responseCode == 403);
    server.request("/api/ranging", HTTP_GET);
    assert(server.responseCode == 403);
    std::cout << "PASS authentication remains required\n";
}

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
wifi_mode_t testMode = WIFI_MODE_STA;
wifi_ps_type_t testPowerSave = WIFI_PS_NONE;
wifi_config_t testAp = {};
int testModeReadError = 0, testPsReadError = 0, testApReadError = 0;
int testModeSetError = 0, testPsSetError = 0, testApSetError = 0;
int testModeWrites = 0, testPsWrites = 0, testApWrites = 0;
bool testIgnoreDriverWrites = false;
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

void configureAp(const char *ssid, bool ftm) {
    testAp = {};
    assert(strlen(ssid) <= sizeof(testAp.ap.ssid));
    testAp.ap.ssid_len = strlen(ssid);
    memcpy(testAp.ap.ssid, ssid, testAp.ap.ssid_len);
    strcpy(reinterpret_cast<char *>(testAp.ap.password), "fixture-private-password");
    testAp.ap.channel = 3;
    testAp.ap.authmode = 3;
    testAp.ap.max_connection = 4;
    testAp.ap.ftm_responder = ftm;
}

void resetDriver() {
    rear_ranging::radioState = rear_ranging::RadioState();
    rear_ranging::pause(false);
    testWifiStatus = WL_CONNECTED;
    testPower = 60;
    testPowerReads = testPowerWrites = 0;
    testMode = WIFI_MODE_STA;
    testPowerSave = WIFI_PS_NONE;
    testModeReadError = testPsReadError = testApReadError = 0;
    testModeSetError = testPsSetError = testApSetError = 0;
    testModeWrites = testPsWrites = testApWrites = 0;
    testIgnoreDriverWrites = false;
    configureAp("Rover provisioning", false);
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

    rear_ranging::updateRadioState();
    assert(testPower == 60 && testPowerWrites == 1);
    rear_ranging::updateRadioState();
    assert(testPowerWrites == 1);
    rear_ranging::writeJson(packet.as<JsonObject>(), 2);
    assert(packet["tx_power_requested_dbm"].as<double>() == 15);
    assert(packet["tx_power_actual_dbm"].as<double>() == 15);
    const int reads = testPowerReads;
    rear_ranging::pause(true);
    rear_ranging::updateRadioState();
    assert(testPowerReads == reads);
    rear_ranging::pause(false);
    testWifiStatus = 0;
    rear_ranging::updateRadioState();
    assert(testPowerReads > reads && testPowerWrites == 1);
    std::cout << "PASS existing station power limit and OTA pause\n";

    allowRequest = false;
    server.request("/api/telemetry/ftm", HTTP_POST);
    assert(server.responseCode == 403);
    server.request("/api/ranging", HTTP_GET);
    assert(server.responseCode == 403);
    std::cout << "PASS authentication remains required\n";
    allowRequest = true;
    auto driverSnapshot = [&]() -> JsonObjectConst {
        packet.clear();
        rear_ranging::writeJson(packet.to<JsonObject>(), 3);
        return packet["wifi_driver"].as<JsonObjectConst>();
    };

    resetDriver();
    testPowerSave = WIFI_PS_MIN_MODEM;
    rear_ranging::updateRadioState();
    auto state = driverSnapshot();
    assert(testPowerSave == WIFI_PS_NONE && testPsWrites == 1);
    assert(state["observed"].as<bool>() && state["mode"].as<int>() == WIFI_MODE_STA);
    assert(state["power_save"].as<int>() == WIFI_PS_NONE);
    assert(state["last_power_save_observed"].as<int>() == WIFI_PS_MIN_MODEM);
    assert(state["power_save_corrections"].as<unsigned>() == 1);
    rear_ranging::updateRadioState();
    assert(testPsWrites == 1); // no repeated write when actual state matches
    assert(driverSnapshot()["sample_sequence"].as<unsigned>() == 2);
    std::cout << "PASS actual sleep mismatch repaired and read back\n";

    resetDriver();
    testMode = WIFI_MODE_APSTA;
    configureAp("C3-TOF-ROVER-REAR-4b858428", true);
    rear_ranging::updateRadioState();
    state = driverSnapshot();
    assert(testMode == WIFI_MODE_STA && !testAp.ap.ftm_responder);
    assert(testModeWrites == 1 && testApWrites == 1 && testWifiStatus == WL_CONNECTED);
    assert(state["legacy_ap_retirements"].as<unsigned>() == 1);
    assert(state["ap_ftm_corrections"].as<unsigned>() == 1);
    assert(state["legacy_ranging_ap_configured"].as<bool>());
    assert(!state["legacy_ranging_ap_active"].as<bool>());
    assert(!state["ap_active"].as<bool>());
    std::cout << "PASS only this board's legacy AP retired with STA retained\n";

    for (const char *ssid : {"Rover provisioning", "C3-TOF-ROVER-REAR-deadbeef"}) {
        resetDriver();
        testMode = WIFI_MODE_APSTA;
        configureAp(ssid, true);
        wifi_config_t expected = testAp;
        expected.ap.ftm_responder = false;
        rear_ranging::updateRadioState();
        state = driverSnapshot();
        assert(testMode == WIFI_MODE_APSTA && testModeWrites == 0);
        assert(memcmp(&testAp, &expected, sizeof(testAp)) == 0);
        assert(state["ap_active"].as<bool>() && !state["ap_ftm_responder"].as<bool>());
        assert(!state["legacy_ranging_ap_configured"].as<bool>());
        std::string json;
        serializeJson(packet, json);
        assert(json.find(ssid) == std::string::npos);
        assert(json.find("fixture-private-password") == std::string::npos);
    }
    std::cout << "PASS provisioning and foreign AP settings preserved without secrets\n";

    resetDriver();
    testMode = WIFI_MODE_AP;
    testWifiStatus = 0;
    testPowerSave = WIFI_PS_MIN_MODEM;
    configureAp("Rover provisioning", true);
    rear_ranging::updateRadioState();
    state = driverSnapshot();
    assert(testModeWrites == 0 && testPsWrites == 0 && testApWrites == 0);
    assert(testPowerWrites == 0);
    assert(state["mode"].as<int>() == WIFI_MODE_AP && !state["station_connected"].as<bool>());
    assert(state["ap_ftm_responder"].as<bool>()); // report actual state, not policy
    assert(state["power_save"].as<int>() == WIFI_PS_MIN_MODEM);
    std::cout << "PASS disconnected provisioning remains mutation-free\n";

    resetDriver();
    testMode = WIFI_MODE_APSTA;
    testPowerSave = WIFI_PS_MIN_MODEM;
    configureAp("C3-TOF-ROVER-REAR-4b858428", true);
    assert(rear_ranging::beginNetworkLoop());
    assert(!rear_ranging::beginNetworkLoop()); // the main owner never waits
    rear_ranging::updateRadioState();
    assert(testModeWrites == 0 && testPsWrites == 0 && testApWrites == 0);
    server.request("/api/ranging", HTTP_GET); // snapshot reads do not reacquire ownership
    assert(server.responseCode == 200);
    state = driverSnapshot();
    assert(!state["observed"].as<bool>() && state["sample_age_ms"].isNull());
    assert(state["ownership_deferrals"].as<unsigned>() == 1);
    // Provisioning replaces the legacy AP while it owns the guard.
    configureAp("Rover provisioning", false);
    rear_ranging::endNetworkLoop();
    rear_ranging::updateRadioState();
    assert(testMode == WIFI_MODE_APSTA && testModeWrites == 0 && testApWrites == 0);
    assert(testPsWrites == 1);
    assert(rear_ranging::beginNetworkLoop()); // successful worker path released ownership
    rear_ranging::endNetworkLoop();
    std::cout << "PASS provisioning ownership prevents stale AP writes\n";

    resetDriver();
    testModeReadError = 0x1234;
    testPsReadError = 0x1235;
    testApReadError = 0x1236;
    rear_ranging::updateRadioState();
    state = driverSnapshot();
    assert(state["mode"].isNull() && state["power_save"].isNull());
    assert(state["ap_active"].isNull() && state["ap_ftm_responder"].isNull());
    assert(state["legacy_ranging_ap_active"].isNull());
    assert(state["mode_read_error"].as<int>() == 0x1234);
    assert(state["power_save_read_error"].as<int>() == 0x1235);
    assert(state["ap_config_read_error"].as<int>() == 0x1236);
    assert(testModeWrites == 0 && testPsWrites == 0 && testApWrites == 0);
    std::cout << "PASS driver read errors stay unknown and do not trigger blind repairs\n";

    for (bool silentlyIgnored : {false, true}) {
        resetDriver();
        testMode = WIFI_MODE_APSTA;
        testPowerSave = WIFI_PS_MIN_MODEM;
        configureAp("C3-TOF-ROVER-REAR-4b858428", true);
        testIgnoreDriverWrites = silentlyIgnored;
        testModeSetError = testPsSetError = testApSetError = silentlyIgnored ? 0 : ESP_ERR_INVALID_STATE;
        rear_ranging::updateRadioState();
        state = driverSnapshot();
        assert(state["mode"].as<int>() == WIFI_MODE_APSTA);
        assert(state["power_save"].as<int>() == WIFI_PS_MIN_MODEM && state["ap_ftm_responder"].as<bool>());
        assert(state["power_save_set_attempts"].as<unsigned>() == 1);
        assert(state["ap_ftm_clear_attempts"].as<unsigned>() == 1);
        assert(state["legacy_ap_retire_attempts"].as<unsigned>() == 1);
        assert(state["power_save_corrections"].as<unsigned>() == 0);
        assert(state["ap_ftm_corrections"].as<unsigned>() == 0);
        assert(state["legacy_ap_retirements"].as<unsigned>() == 0);
        assert(state["last_power_save_set_error"].as<int>() == testPsSetError);
        assert(state["last_ap_ftm_clear_error"].as<int>() == testApSetError);
        assert(state["last_legacy_ap_retire_error"].as<int>() == testModeSetError);
    }
    std::cout << "PASS failed or ineffective writes never count as corrections\n";

}

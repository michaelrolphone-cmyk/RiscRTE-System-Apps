#pragma once
constexpr int WL_CONNECTED = 3;
extern int testWifiStatus;
struct TestWiFi { int status() const { return testWifiStatus; } };
extern TestWiFi WiFi;

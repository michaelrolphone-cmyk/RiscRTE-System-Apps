#pragma once

namespace rear_ota_recovery {
bool begin(const char *hostname, const char *ssid, const char *wifiPassword);
bool enrolled();
const char *bootIdentity();
void service();
}

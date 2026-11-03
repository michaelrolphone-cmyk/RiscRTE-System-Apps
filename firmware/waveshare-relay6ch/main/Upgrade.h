#pragma once
#include "Digest.h"

namespace RelayBoard {
inline bool matchesDigest(JsonVariantConst value, const char* expected) {
  if (value.isNull()) return false;
  char actual[65];
  Infra::canonicalDigest(value, actual);
  return strcmp(actual, expected) == 0;
}

// Only the exact interim provisioning file gained these two signing references.
// Shared lease membership hashes the subscriptions, even for bearer-only callbacks.
inline void restoreSubscriptions(JsonObject relay) {
  if (!matchesDigest(relay["subscriptions"],
        "814eac67fac68ee25742e5ab09826666f776242f90b9251563173fbac7b607cf")) return;
  for (JsonObject subscription : relay["subscriptions"].as<JsonArray>())
    subscription.remove("signing_credential_ref");
}

inline void upgradeSetup(JsonObject policy, JsonObjectConst factory, JsonObject relayState) {
  auto service = policy["service"].as<JsonObject>();
  if (matchesDigest(service["providers"],
        "7e16f5362d896f31437032d271d8967216a3d9163246f06a7440e6caa688f377") &&
      matchesDigest(service["contracts"],
        "154cdb7b67123f1224594aa13dd909ecb5465ab248980a31b563437af3901d25"))
    service["providers"] = factory["service"]["providers"];
  restoreSubscriptions(policy["relay"].as<JsonObject>());
  // Never edit an outstanding delivery or an independently updated subscription.
  restoreSubscriptions(relayState);
}
}

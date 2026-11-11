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
  if ((matchesDigest(service["providers"],
        "7e16f5362d896f31437032d271d8967216a3d9163246f06a7440e6caa688f377") ||
       matchesDigest(service["providers"],
        "076acb5693f48e4679cc709e8a9912038657e5d70d98ed48c814a479ad2f3666") ||
       matchesDigest(service["providers"],
        "5609c7cfd0bbce9c7f1ca41bc00ad5675f6198387d0b2cdbcbeccd6c414a0d9b")) &&
      matchesDigest(service["contracts"],
        "154cdb7b67123f1224594aa13dd909ecb5465ab248980a31b563437af3901d25")) {
    service["contracts"] = factory["service"]["contracts"];
    service["providers"] = factory["service"]["providers"];
  }
  // v1.0.4's exact nine-provider cohort can move to this firmware's pins.
  // Retain destination URLs, credentials, allocation and all delivery custody.
  if ((matchesDigest(service["providers"], "82ed5ff88955a866b4e06fb8f258bf7c9b371014f4190ee1bc4609ab430de0a9") ||
       matchesDigest(service["providers"], "c4d47f4c46b4f39c309f7dbb1cc0a6460967ed8d1ceb5b4f7d019328af4ee9ba")) &&
      matchesDigest(service["contracts"], "2e3fe7875d33e2ba17dfd56bec022a04f3721e8f1af24eba04148bfa676328f9"))
    service["providers"] = factory["service"]["providers"];
  restoreSubscriptions(policy["relay"].as<JsonObject>());
  // Never edit an outstanding delivery or an independently updated subscription.
  restoreSubscriptions(relayState);
}
}

#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
runtime="${RISC_RUNTIME_ROOT:-$root/../runtime}"
build="$(mktemp -d)"
trap 'rm -rf "$build"' EXIT
flags=(-std=c++17 -Wno-deprecated-declarations -I"$runtime/lib/ArduinoJson/src" -I"$runtime/sdk/app" -I"$runtime/sdk/driver" -I"$root/Services/infra_node")
if [[ "${SANITIZE:-0}" == 1 ]];then flags+=(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer);fi
for name in core policy journal transport boot_health;do
 c++ "${flags[@]}" "$root/test/infra_node/$name.cpp" -o "$build/$name"
 if [[ "$name" == policy ]];then "$build/$name" "$root/profiles/esp32s3-infrastructure/fixtures/node-policy.example.json";else "$build/$name";fi
done

c++ "${flags[@]}" -DWITH_SHA256 -I"$runtime/sdk/hardware" "$root/test/infra_node/portal.cpp" "$root/Services/infra_node/Node.cpp" "$root/Services/infra_node/Setup.cpp" "$root/lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c" -o "$build/portal"
"$build/portal" "$root/profiles/esp32s3-infrastructure/fixtures/node-policy.example.json"

if [[ -n "${CAPABILITIES_SOURCE:-}" && -n "${SCHEDULER_SOURCE:-}" ]];then
 c++ "${flags[@]}" -DWITH_SHA256 -I"$runtime/sdk/hardware" "$root/test/infra_node/node.cpp" "$root/Services/infra_node/Node.cpp" "$root/Services/infra_node/Setup.cpp" "$root/lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c" -o "$build/node"
 node "$root/scripts/test_infrastructure_service.mjs" "$build/node" "$CAPABILITIES_SOURCE" "$SCHEDULER_SOURCE"
fi

#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
runtime="${RISC_RUNTIME_ROOT:-$root/../runtime}"
build="$(mktemp -d)"
trap 'rm -rf "$build"' EXIT
flags=(-std=c++17 -Wno-deprecated-declarations -DWITH_SHA256
 -I"$runtime/lib/ArduinoJson/src" -I"$runtime/sdk/app" -I"$runtime/sdk/driver"
 -I"$runtime/sdk/hardware" -I"$root/Services/infra_node")
crypto_flags=()
if [[ "${SANITIZE:-0}" == 1 ]];then
 flags+=(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
 crypto_flags+=(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
fi
cc "${crypto_flags[@]}" -c "$root/lib/EnrollmentCrypto/monocypher.c" -o "$build/crypto.o"
c++ "${flags[@]}" "$root/test/infra_node/biome_integration.cpp" \
 "$root/Services/infra_node/Node.cpp" "$root/Services/infra_node/Relay.cpp" \
 "$root/Services/infra_node/Enrollment.cpp" "$root/Services/infra_node/Setup.cpp" \
 "$root/lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c" "$build/crypto.o" -o "$build/integration"
"$build/integration" "$root/profiles/waveshare-esp32-s3-relay-6ch/node-policy.example.json" \
 "$root/test/infra_node/fixtures/relay6ch-v104-pins.json"

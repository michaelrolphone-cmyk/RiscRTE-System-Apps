#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
runtime="${RISC_RUNTIME_ROOT:-$root/../runtime}"
build="$(mktemp -d)";trap 'rm -rf "$build"' EXIT
flags=(-std=c++17 -Wno-deprecated-declarations -DWITH_SHA256 -I"$runtime/lib/ArduinoJson/src" -I"$runtime/sdk/app" -I"$runtime/sdk/driver" -I"$runtime/sdk/hardware" -I"$root/Services/infra_node")
if [[ "${SANITIZE:-0}" == 1 ]];then flags+=(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer);fi
cc -c "$root/lib/EnrollmentCrypto/monocypher.c" -o "$build/crypto.o"
c++ "${flags[@]}" "$root/test/infra_node/hardware.cpp" "$root/Services/infra_node/Node.cpp" "$root/Services/infra_node/Relay.cpp" "$root/Services/infra_node/Enrollment.cpp" "$root/Services/infra_node/Setup.cpp" "$root/lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c" "$build/crypto.o" -o "$build/test"
"$build/test" "$root/profiles/waveshare-esp32-s3-relay-6ch/node-policy.example.json"

# Exercise the actual board runner and PWM return-code convention with mock GPIO.
c++ "${flags[@]}" -pthread -I"$root/test/relay6ch_host" -I"$root/firmware/waveshare-relay6ch/main" "$root/test/relay6ch_host/check.cpp" "$root/firmware/waveshare-relay6ch/main/Board.cpp" -o "$build/board"
timeout 10 "$build/board"

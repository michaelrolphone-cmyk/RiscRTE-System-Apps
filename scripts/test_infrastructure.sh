#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
runtime="${RISC_RUNTIME_ROOT:-$root/../runtime}"
RISC_RUNTIME_ROOT="$runtime" bash "$root/scripts/test_infrastructure_station.sh"
node "$root/test/infra_node/portal_page.mjs"
build="$(mktemp -d)"
trap 'rm -rf "$build"' EXIT
flags=(-std=c++17 -Wno-deprecated-declarations -I"$runtime/lib/ArduinoJson/src" -I"$runtime/sdk/app" -I"$runtime/sdk/driver" -I"$root/Services/infra_node")
if [[ "${SANITIZE:-0}" == 1 ]];then flags+=(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer);fi
crypto_flags=();if [[ "${SANITIZE:-0}" == 1 ]];then crypto_flags=(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer);fi
cc "${crypto_flags[@]}" -c "$root/lib/EnrollmentCrypto/monocypher.c" -o "$build/monocypher.o"
for name in core policy journal transport boot_health;do
 c++ "${flags[@]}" "$root/test/infra_node/$name.cpp" -o "$build/$name"
 if [[ "$name" == policy ]];then "$build/$name" "$root/profiles/esp32s3-infrastructure/fixtures/node-policy.example.json";else "$build/$name";fi
done

c++ "${flags[@]}" -I"$runtime/src/runtime/storage" "$root/test/infra_node/journal_backend.cpp" "$runtime/src/runtime/storage/AppDataFiles.cpp" -o "$build/journal_backend"
"$build/journal_backend"

c++ "${flags[@]}" -DWITH_SHA256 -I"$runtime/sdk/hardware" "$root/test/infra_node/portal.cpp" "$root/Services/infra_node/Node.cpp" "$root/Services/infra_node/Relay.cpp" "$root/Services/infra_node/Enrollment.cpp" "$build/monocypher.o" "$root/Services/infra_node/Setup.cpp" "$root/lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c" -o "$build/portal"
"$build/portal" "$root/profiles/esp32s3-infrastructure/fixtures/node-policy.example.json" "$root/test/infra_node/fixtures/portal-v122-providers.json"
"$build/portal" "$root/profiles/esp32s3-infrastructure/fixtures/node-policy.example.json" "$root/test/infra_node/fixtures/portal-v123-providers.json"
"$build/portal" "$root/profiles/esp32s3-infrastructure/fixtures/node-policy.example.json" "$root/test/infra_node/fixtures/portal-v140-providers.json"
"$build/portal" "$root/profiles/esp32s3-infrastructure/fixtures/node-policy.example.json" "$root/test/infra_node/fixtures/portal-v141-providers.json"

 c++ "${flags[@]}" -DWITH_SHA256 -I"$runtime/sdk/hardware" "$root/test/infra_node/relay.cpp" "$root/Services/infra_node/Node.cpp" "$root/Services/infra_node/Relay.cpp" "$root/Services/infra_node/Enrollment.cpp" "$build/monocypher.o" "$root/Services/infra_node/Setup.cpp" "$root/lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c" -o "$build/relay"
 "$build/relay" "$root/profiles/esp32s3-infrastructure/fixtures/node-policy.example.json"
 c++ "${flags[@]}" -DWITH_SHA256 -I"$runtime/sdk/hardware" "$root/test/infra_node/rejection.cpp" "$root/Services/infra_node/Node.cpp" "$root/Services/infra_node/Relay.cpp" "$root/Services/infra_node/Enrollment.cpp" "$root/Services/infra_node/Setup.cpp" "$build/monocypher.o" "$root/lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c" -o "$build/rejection"
 "$build/rejection" "$root/profiles/esp32s3-infrastructure/fixtures/node-policy.example.json"
 node "$root/test/infra_node/enrollment_vectors.mjs" "$build/enrollment-vectors.json"
 c++ "${flags[@]}" -DWITH_SHA256 -I"$runtime/sdk/hardware" "$root/test/infra_node/enrollment.cpp" "$root/Services/infra_node/Node.cpp" "$root/Services/infra_node/Relay.cpp" "$root/Services/infra_node/Enrollment.cpp" "$root/Services/infra_node/Setup.cpp" "$build/monocypher.o" "$root/lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c" -o "$build/enrollment"
 "$build/enrollment" "$root/profiles/esp32s3-infrastructure/fixtures/node-policy.example.json" "$build/enrollment-vectors.json"
 python3 - <<'HMAC'
import json,hmac,hashlib
v=json.load(open('/tmp/infra-relay-hmac-vector.json'))
assert v['signature']=='sha256='+hmac.new(b'signing-secret',(v['timestamp']+'.'+v['body']).encode(),hashlib.sha256).hexdigest()
print('Relay HMAC: independently verified exact body and timestamp PASS')
HMAC

if [[ -n "${CAPABILITIES_SOURCE:-}" && -n "${SCHEDULER_SOURCE:-}" ]];then
 c++ "${flags[@]}" -DWITH_SHA256 -I"$runtime/sdk/hardware" "$root/test/infra_node/node.cpp" "$root/Services/infra_node/Node.cpp" "$root/Services/infra_node/Relay.cpp" "$root/Services/infra_node/Enrollment.cpp" "$build/monocypher.o" "$root/Services/infra_node/Setup.cpp" "$root/lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c" -o "$build/node"
 node "$root/scripts/test_infrastructure_service.mjs" "$build/node" "$CAPABILITIES_SOURCE" "$SCHEDULER_SOURCE"
fi

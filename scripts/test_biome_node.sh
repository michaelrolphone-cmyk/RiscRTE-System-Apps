#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
runtime="${RISC_RUNTIME_ROOT:-$root/../runtime}"
build="$(mktemp -d)"
trap 'rm -rf "$build"' EXIT
flags=(-std=c++17 -Wno-deprecated-declarations -DWITH_SHA256
 -I"$runtime/lib/ArduinoJson/src" -I"$runtime/sdk/app" -I"$runtime/sdk/driver" -I"$runtime/sdk/hardware")
if [[ "${SANITIZE:-0}" == 1 ]];then
 flags+=(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
fi
c++ "${flags[@]}" "$root/test/biome_node/state.cpp" "$root/lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c" -o "$build/state"
c++ "${flags[@]}" "$root/test/biome_node/encoding.cpp" "$root/lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c" -o "$build/encoding"
"$build/state"
"$build/encoding"
python3 - "$build/encoding" <<'PY'
import base64
import hashlib
import json
import subprocess
import sys

rows = subprocess.check_output([sys.argv[1], "--vectors"], text=True).splitlines()
assert len(rows) == 5
# Keep control/Unicode fixture input independent of ArduinoJson's serializer.
# The wire rows carry only ASCII canonical strings and digest identifiers.
vectors = [
    {"z": "line\n\t\"\\ \u00e9 \U0001f600\x7f", "a": True,
     "negative": -123, "empty": None, "quote\"name": "\x01"},
    {"site": "f45540bf9910412680734d0613e84460", "realm": "site",
     "id": "request-1", "operation": "provider.result",
     "arguments": {"provider": "a" * 64, "operation": "action-1",
                   "generation": 9007199254740991, "state": "succeeded",
                   "result": {"channel": 6, "on": False}}},
    {"challenge": "0" * 64, "site": "human.site", "key": "b" * 64},
    {"arguments": {"inputs": {"steps": [
        {"on": [1], "off": [2], "wait_ms": 137}, {"tone_hz": 0}]}}},
]
for row in map(json.loads, rows):
    if "vector" in row:
        canonical = json.dumps(vectors[row["vector"]], sort_keys=True, separators=(",", ":"), ensure_ascii=True, allow_nan=False)
        assert row["canonical"] == canonical, (row["canonical"], canonical)
        assert row["sha256"] == hashlib.sha256(canonical.encode()).hexdigest()
    else:
        der = base64.b64decode(row["spki"], validate=True)
        assert len(der) == 294
        assert der[:24] == bytes.fromhex("30820122300d06092a864886f70d01010105000382010f00")
        assert row["fingerprint"] == hashlib.sha256(der).hexdigest()
        pem_body = "".join(row["pem"].splitlines()[1:-1])
        assert base64.b64decode(pem_body, validate=True) == der
        # Independent ASN.1/RSA parser checks the exact exported public format.
        result = subprocess.run(["openssl", "pkey", "-pubin", "-inform", "DER", "-noout"],
                                input=der, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        assert result.returncode == 0, result.stderr.decode()
print("Biome interoperability: native Python canonical/signing digest and OpenSSL SPKI parsing PASS")
PY

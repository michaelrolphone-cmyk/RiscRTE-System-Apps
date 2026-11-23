#!/usr/bin/env python3
"""Restore ESP32 WebSocket Stream timeout units in the pinned rear dependency.

WebSockets 2.7.2 passes milliseconds to WiFiClient 2.0.17's seconds API.
Only the project's rear dependency is patched. Exact library and framework
blobs guard that API assumption; repeat builds rederive the reviewed output.
"""
import base64
import hashlib
import json
import os
from pathlib import Path

ORIGINAL = {
    "WebSocketsClient.cpp": "c9d09e84247d260bdd2b50404e24a2423fb45bc8",
    "WebSockets.h": "51ee3efe2d336983f1dbf1e2d24f84f2eb36b7b4",
}
FRAMEWORK = {
    "cores/esp32/Stream.cpp": "f412e46119552430cae6ff74a86da9f509d343d3",
    "libraries/WiFi/src/WiFiClient.cpp": "a4301ad0e09e2d460db158ce620a26cf46cc0e73",
}
RECEIPT = "rear-websocket-timeout-patch.json"


def git_blob(data):
    return hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()


def framework_root():
    core = Path(os.environ.get("PLATFORMIO_CORE_DIR", str(Path.home() / ".platformio")))
    packages = Path(os.environ.get("PLATFORMIO_PACKAGES_DIR", str(core / "packages")))
    return packages / "framework-arduinoespressif32"


def validate_framework(root):
    root = Path(root)
    for path, expected in FRAMEWORK.items():
        if git_blob((root / path).read_bytes()) != expected:
            raise ValueError("Unreviewed ESP32 timeout API source: " + path)
    return root


def locate(prepared):
    prepared = Path(prepared).resolve()
    matches = list((prepared / ".pio/libdeps/esp32cam_s3_wroom_n16r8").glob(
        "*/src/WebSocketsClient.cpp"))
    if len(matches) != 1:
        raise ValueError("Resolve the one pinned WebSockets 2.7.2 rear dependency first")
    root = matches[0].parent.resolve()
    if prepared not in root.parents:
        raise ValueError("WebSocket dependency must be local to the prepared rear project")
    for name in ORIGINAL:
        if prepared not in (root / name).resolve().parents:
            raise ValueError("WebSocket dependency must be local to the prepared rear project")
    return root


def validate_original(original):
    if set(original) != set(ORIGINAL):
        raise ValueError("Unexpected original WebSocket source set")
    for name, expected in ORIGINAL.items():
        if git_blob(original[name]) != expected:
            raise ValueError("Unreviewed WebSocket dependency bytes: " + name)


def transform(original):
    validate_original(original)
    patched = dict(original)
    source = original["WebSocketsClient.cpp"]
    old = b"    _client.tcp->setTimeout(WEBSOCKETS_TCP_TIMEOUT);"
    if source.count(old) != 1:
        raise ValueError("WebSocket timeout assignment context changed")
    newline = b"\r\n" if b"\r\n" in source else b"\n"
    new = newline.join(line.encode() for line in (
        "#if defined(ESP32)",
        "    // WiFiClient 2.0.17 takes seconds; Stream takes milliseconds.",
        "    // Keep the existing idle deadline, including for partial HTTP lines.",
        "    _client.tcp->setTimeout((WEBSOCKETS_TCP_TIMEOUT + 999) / 1000);",
        "    _client.tcp->Stream::setTimeout(WEBSOCKETS_TCP_TIMEOUT);",
        "#else",
        "    _client.tcp->setTimeout(WEBSOCKETS_TCP_TIMEOUT);",
        "#endif",
    ))
    patched["WebSocketsClient.cpp"] = source.replace(old, new)
    return patched


def apply(prepared, framework=None):
    framework = validate_framework(framework if framework is not None else framework_root())
    root = locate(prepared)
    receipt = root.parent / RECEIPT
    saved = json.loads(receipt.read_text()) if receipt.exists() else None
    if saved is not None:
        if (saved.get("schema") != 1 or saved.get("original_git_blobs") != ORIGINAL or
                saved.get("framework_git_blobs") != FRAMEWORK):
            raise ValueError("Unexpected WebSocket timeout patch receipt")
        original = {name: base64.b64decode(value, validate=True)
                    for name, value in saved.get("original_source_base64", {}).items()}
    else:
        original = {name: (root / name).read_bytes() for name in ORIGINAL}
    patched = transform(original)
    hashes = {name: hashlib.sha256(value).hexdigest() for name, value in patched.items()}
    if saved is not None:
        if saved.get("sha256") != hashes:
            raise ValueError("WebSocket receipt differs from reviewed transformation")
        for name in ORIGINAL:
            if (root / name).read_bytes() != patched[name]:
                raise ValueError("Patched WebSocket dependency changed: " + name)
    else:
        # Validate every library/framework input before changing the local copy.
        for name, value in patched.items():
            if value != original[name]:
                (root / name).write_bytes(value)
        saved = {
            "schema": 1, "library": "WebSockets@2.7.2",
            "original_git_blobs": ORIGINAL, "framework_git_blobs": FRAMEWORK,
            "sha256": hashes,
            "original_source_base64": {
                name: base64.b64encode(value).decode() for name, value in original.items()
            },
        }
        temporary = receipt.with_suffix(".tmp")
        temporary.write_text(json.dumps(saved, indent=2) + "\n")
        temporary.replace(receipt)
    return {key: value for key, value in saved.items() if key != "original_source_base64"}


if "Import" in globals():
    Import("env")
    apply(Path(env.subst("$PROJECT_DIR")),
          Path(env.PioPlatform().get_package_dir("framework-arduinoespressif32")))
elif __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("prepared", type=Path)
    parser.add_argument("--framework", type=Path)
    args = parser.parse_args()
    print(json.dumps(apply(args.prepared, args.framework), indent=2))

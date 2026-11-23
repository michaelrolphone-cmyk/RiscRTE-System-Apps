#!/usr/bin/env python3
"""Exercise exact WebSockets/ESP32 timeout methods at fake clock/socket boundaries.

This is a unit-conversion regression, not a full Wi-Fi or incremental-reader
simulation. It compiles verbatim methods from the checksum-verified production
dependencies, and the same partial-header test must fail on the unchanged code.
"""
import argparse
import base64
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

from patch_websocket_library import (
    FRAMEWORK, ORIGINAL, RECEIPT, apply, framework_root, locate,
    transform, validate_framework, validate_original,
)

METHODS = {
    "WebSocketsClient.cpp": [
        "void WebSocketsClient::connectedCb()",
        "void WebSocketsClient::handleClientData(void)",
    ],
    "Stream.cpp": [
        "int Stream::timedRead()",
        "void Stream::setTimeout(unsigned long timeout)",
        "unsigned long Stream::getTimeout(void)",
        "size_t Stream::readBytes(char *buffer, size_t length)",
        "String Stream::readStringUntil(char terminator)",
    ],
    "WiFiClient.cpp": ["int WiFiClient::setTimeout(uint32_t seconds)"],
}
CONSTANTS = (
    "WEBSOCKETS_TCP_TIMEOUT", "NETWORK_ESP8266_ASYNC", "NETWORK_ESP8266",
    "NETWORK_ESP32", "NETWORK_RP2040",
)


def method(data, signature):
    text = data.decode().replace("\r\n", "\n")
    if text.count(signature) != 1:
        raise AssertionError("Production method signature changed: " + signature)
    start = text.index(signature)
    opening = text.index("{", start)
    # Each reviewed method ends with a column-zero brace. Its nested scopes
    # are indented; the full source-blob guard makes this extraction exact.
    end = text.index("\n}", opening) + 2
    return text[start:end] + "\n"


def original_library(root):
    receipt = root.parent / RECEIPT
    if not receipt.exists():
        original = {name: (root / name).read_bytes() for name in ORIGINAL}
    else:
        saved = json.loads(receipt.read_text())
        if saved.get("schema") != 1 or saved.get("original_git_blobs") != ORIGINAL:
            raise AssertionError("Unexpected prepared timeout receipt")
        original = {name: base64.b64decode(value, validate=True)
                    for name, value in saved.get("original_source_base64", {}).items()}
        expected = transform(original)
        if any((root / name).read_bytes() != value for name, value in expected.items()):
            raise AssertionError("Prepared patched bytes differ from reviewed transformation")
    validate_original(original)
    return original


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prepared", required=True, type=Path)
    parser.add_argument("--framework", type=Path)
    args = parser.parse_args()
    framework = validate_framework(args.framework or framework_root())
    production = locate(args.prepared)
    original = original_library(production)
    framework_data = {
        Path(path).name: (framework / path).read_bytes() for path in FRAMEWORK
    }
    before = {name: (production / name).read_bytes() for name in ORIGINAL}
    compiler = shutil.which("c++")
    if not compiler:
        raise SystemExit("A host C++ compiler is required")
    fixture = Path(__file__).resolve().parent / "overlay/test/websocket_timeout/test_timeout.cpp"
    with tempfile.TemporaryDirectory(prefix="rear-websocket-timeout-") as directory:
        work = Path(directory)
        package = work / ".pio/libdeps/esp32cam_s3_wroom_n16r8/WebSockets/src"
        package.mkdir(parents=True)
        for name, value in original.items():
            (package / name).write_bytes(value)
        apply(work, framework)
        patched = {name: (package / name).read_bytes() for name in ORIGINAL}

        header = original["WebSockets.h"].decode().replace("\r\n", "\n")
        definitions = []
        for name in CONSTANTS:
            matches = re.findall(r"^#define " + name + r" .+$", header, re.M)
            if len(matches) != 1:
                raise AssertionError("Expected one actual protocol constant: " + name)
            definitions.append(matches[0])
        (work / "actual_constants.inc").write_text("\n".join(definitions) + "\n")

        def compile_variant(name, library):
            sources = dict(framework_data)
            sources["WebSocketsClient.cpp"] = library["WebSocketsClient.cpp"]
            body = "".join(method(sources[path], signature)
                           for path, signatures in METHODS.items() for signature in signatures)
            (work / "actual_methods.inc").write_text(body)
            executable = work / name
            subprocess.run([
                compiler, "-std=c++11", "-Wall", "-Wextra", "-Werror",
                "-I", str(work), str(fixture), "-o", str(executable),
            ], check=True)
            return executable

        unchanged = compile_variant("unchanged", original)
        control = subprocess.run([str(unchanged), "partial-header"],
                                 text=True, capture_output=True, timeout=15)
        print(control.stdout, end="")
        if (control.returncode != 1 or
                "partial_header_elapsed_ms=5000000" not in control.stdout or
                "partial header exceeded intended 5000 ms idle timeout" not in control.stderr):
            raise AssertionError("The unchanged dependency did not reproduce the expected timeout defect")
        print("PASS unchanged-source negative control: expected 5000 ms bound fails at 5000000 ms")

        candidate = compile_variant("patched", patched)
        subprocess.run([str(candidate)], check=True, timeout=15)

        receipt = package.parent / RECEIPT
        before_repeat = receipt.read_bytes()
        apply(work, framework)
        if receipt.read_bytes() != before_repeat:
            raise AssertionError("Repeated patch altered its receipt")
        print("PASS repeated dependency patch verifies identical reviewed bytes")

        target = package / "WebSocketsClient.cpp"
        target.write_bytes(target.read_bytes() + b"\n// unexpected drift\n")
        try:
            apply(work, framework)
        except ValueError:
            pass
        else:
            raise AssertionError("Patched source drift was accepted")
        print("PASS patched source drift rejected")

        saved = json.loads(receipt.read_text())
        saved["sha256"]["WebSocketsClient.cpp"] = hashlib.sha256(target.read_bytes()).hexdigest()
        receipt.write_text(json.dumps(saved))
        try:
            apply(work, framework)
        except ValueError:
            pass
        else:
            raise AssertionError("Forged output digest bypassed reviewed transformation")
        print("PASS forged output digest rejected")

    if any((production / name).read_bytes() != value for name, value in before.items()):
        raise AssertionError("Read-only production dependency was changed by host tests")
    print("Actual-method timeout regression and 3 patch guards passed; supplied prepared source unchanged")


if __name__ == "__main__":
    main()

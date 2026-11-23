#!/usr/bin/env python3
"""Bound rear VL53L5CX recovery and reject incomplete reads in pinned v1.0.3.

Only the prepared rear dependency is changed. Original Git blobs guard the
input; the receipt retains those exact inputs so repeat builds recompute and
verify the reviewed transformation rather than trusting recorded output hashes.
"""
import hashlib
import json
from pathlib import Path
import re

ORIGINAL = {
    "vl53l5cx_api.cpp": "4b54f0d2bb7d5fc2d437eb03bbc1af385b5ae88d",
    "SparkFun_VL53L5CX_IO.cpp": "effb1690aae23f4a70229af61f646c9c272a7e18",
    "platform.cpp": "d61d0a2e52d2e68528bd859910936a7fa29f81b7",
}
RECEIPT = "rear-lidar-safety-patch.json"
# Current rear init/config/start call chain. Stop at the first failed operation;
# otherwise several individually bounded polls still accumulate in one attempt.
FAIL_FAST = {
    "_vl53l5cx_send_offset_data": 2,
    "_vl53l5cx_send_xtalk_data": 2,
    "vl53l5cx_is_alive": 4,
    "vl53l5cx_init": 81,
    "vl53l5cx_get_resolution": 1,
    "vl53l5cx_set_resolution": 10,
    "vl53l5cx_set_ranging_frequency_hz": 1,
    "vl53l5cx_start_ranging": 9,
    "vl53l5cx_dci_read_data": 4,
    "vl53l5cx_dci_write_data": 3,
    "vl53l5cx_dci_replace_data": 2
}

def git_blob(data):
    return hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()

def once(text, old, new):
    if text.count(old) != 1:
        raise ValueError("LiDAR patch context changed: " + old[:80])
    return text.replace(old, new)

def transform(original):
    texts = dict(original)
    api = texts["vl53l5cx_api.cpp"]
    api = once(api, "static uint8_t _vl53l5cx_poll_for_answer(VL53L5CX_Configuration *p_dev, uint8_t size, uint8_t pos, uint16_t address, uint8_t mask, uint8_t expected_value)\n{\n\tuint8_t status = VL53L5CX_STATUS_OK;\n\tuint8_t timeout = 0;\n\n\tdo\n\t{\n\t\tstatus |= RdMulti(&(p_dev->platform), address, p_dev->temp_buffer, size);\n\t\tstatus |= WaitMs(&(p_dev->platform), 10);\n\n\t\tif (timeout >= (uint8_t)200) /* 2s timeout */\n\t\t{\n\t\t\tstatus |= p_dev->temp_buffer[2];\n\t\t}\n\t\telse\n\t\t{\n\t\t\tif ((size >= (uint8_t)4) && (p_dev->temp_buffer[2] >= (uint8_t)0x7f))\n\t\t\t{\n\t\t\t\tstatus |= VL53L5CX_MCU_ERROR;\n\t\t\t}\n\t\t\telse\n\t\t\t{\n\t\t\t\ttimeout++;\n\t\t\t}\n\t\t}\n\t} while ((p_dev->temp_buffer[pos] & mask) != expected_value);\n\n\treturn status;\n}", "static uint8_t _vl53l5cx_poll_for_answer(VL53L5CX_Configuration *p_dev, uint8_t size, uint8_t pos, uint16_t address, uint8_t mask, uint8_t expected_value)\n{\n\tconst uint32_t started = millis();\n\tfor (;;)\n\t{\n\t\t// A sensor that still ACKs I2C must not hold both rear heads forever.\n\t\tif ((uint32_t)(millis() - started) >= 2000U)\n\t\t\treturn VL53L5CX_STATUS_ERROR;\n\t\tuint8_t status = RdMulti(&(p_dev->platform), address, p_dev->temp_buffer, size);\n\t\tif (status != VL53L5CX_STATUS_OK) return status;\n\t\tstatus = WaitMs(&(p_dev->platform), 10);\n\t\tif (status != VL53L5CX_STATUS_OK) return status;\n\t\tif ((size >= (uint8_t)4) && (p_dev->temp_buffer[2] >= (uint8_t)0x7f))\n\t\t\treturn VL53L5CX_MCU_ERROR;\n\t\tif ((p_dev->temp_buffer[pos] & mask) == expected_value)\n\t\t\treturn VL53L5CX_STATUS_OK;\n\t}\n}")
    for name, expected in FAIL_FAST.items():
        start = api.index("uint8_t " + name + "(")
        end = api.index("\n}", start) + 2
        body = api[start:end]
        def guarded(match):
            # A single do/while statement preserves conditional/case scope.
            expression = match.group(1)
            return ("do { status = " + expression +
                    "; if (status != VL53L5CX_STATUS_OK) return status; } while (0);")
        changed, count = re.subn(r"status \|= ([^;]+);", guarded, body)
        if count != expected:
            raise ValueError("LiDAR fail-fast statement count changed: " + name)
        api = api[:start] + changed + api[end:]
    api = once(api,
        "uint8_t vl53l5cx_is_alive(VL53L5CX_Configuration *p_dev, uint8_t *p_is_alive)\n{",
        "uint8_t vl53l5cx_is_alive(VL53L5CX_Configuration *p_dev, uint8_t *p_is_alive)\n{\n\t*p_is_alive = 0;")
    api = once(api,
        "\tstatus |= RdMulti(&(p_dev->platform), 0x0, p_dev->temp_buffer, 4);",
        "\t*p_isReady = 0;\n\tstatus = RdMulti(&(p_dev->platform), 0x0, p_dev->temp_buffer, 4);\n\tif (status != VL53L5CX_STATUS_OK) return status;")
    api = once(api,
        "\tstatus |= RdMulti(&(p_dev->platform), 0x0, p_dev->temp_buffer, p_dev->data_read_size);",
        "\tstatus = RdMulti(&(p_dev->platform), 0x0, p_dev->temp_buffer, p_dev->data_read_size);\n\tif (status != VL53L5CX_STATUS_OK) return status;")
    texts["vl53l5cx_api.cpp"] = api
    texts["SparkFun_VL53L5CX_IO.cpp"] = once(
        texts["SparkFun_VL53L5CX_IO.cpp"], "        _i2cPort->requestFrom((uint8_t)_address, (uint8_t)bytesToRead);\n        if (_i2cPort->available())\n        {\n            for (uint16_t x = 0; x < bytesToRead; x++)\n                buffer[offset + x] = _i2cPort->read();\n        }\n        else\n            return (false); // Sensor did not respond", "        // Wire returns a byte count; zero is an error here, not status success.\n        const size_t received = _i2cPort->requestFrom((uint8_t)_address, (uint8_t)bytesToRead);\n        if (received != bytesToRead)\n            return 4;\n        for (uint16_t x = 0; x < bytesToRead; x++)\n        {\n            const int value = _i2cPort->read();\n            if (value < 0)\n                return 4;\n            buffer[offset + x] = static_cast<uint8_t>(value);\n        }")
    texts["platform.cpp"] = once(texts["platform.cpp"],
        "\t*p_value = p_platform->VL53L5CX_i2c->readSingleByte(RegisterAdress);\n\treturn 0;",
        "\treturn p_platform->VL53L5CX_i2c->readMultipleBytes(RegisterAdress, p_value, 1);")
    return texts

def locate(prepared):
    roots = list((Path(prepared) / ".pio/libdeps/esp32cam_s3_wroom_n16r8").glob(
        "*/src/vl53l5cx_api.cpp"))
    if len(roots) != 1:
        raise ValueError("Resolve the one pinned SparkFun VL53L5CX 1.0.3 dependency first")
    return roots[0].parent

def validate_original(texts):
    if set(texts) != set(ORIGINAL):
        raise ValueError("Unexpected original LiDAR source set")
    for name, expected in ORIGINAL.items():
        if git_blob(texts[name].encode()) != expected:
            raise ValueError("Unreviewed LiDAR dependency bytes: " + name)

def apply(prepared):
    root = locate(prepared)
    receipt = root.parent / RECEIPT
    saved = json.loads(receipt.read_text()) if receipt.exists() else None
    if saved is not None:
        if saved.get("schema") != 1 or saved.get("original_git_blobs") != ORIGINAL:
            raise ValueError("Unexpected LiDAR dependency patch receipt")
        original = saved.get("original_source", {})
    else:
        original = {name: (root/name).read_text() for name in ORIGINAL}
    validate_original(original)
    patched = transform(original)
    hashes = {name: hashlib.sha256(text.encode()).hexdigest()
              for name, text in patched.items()}
    if saved is not None:
        if saved.get("sha256") != hashes:
            raise ValueError("LiDAR receipt does not match reviewed transformation")
        for name in ORIGINAL:
            if (root/name).read_bytes() != patched[name].encode():
                raise ValueError("Patched LiDAR dependency changed: " + name)
    else:
        # Validate/transform every input before making any mutation.
        for name, text in patched.items():
            (root/name).write_bytes(text.encode())
        saved = {"schema": 1, "library": "SparkFun VL53L5CX Arduino Library@1.0.3",
                 "original_git_blobs": ORIGINAL, "sha256": hashes,
                 "original_source": original}
        temporary = receipt.with_suffix(".tmp")
        temporary.write_text(json.dumps(saved, indent=2) + "\n")
        temporary.replace(receipt)
    return {key: value for key, value in saved.items() if key != "original_source"}

if "Import" in globals():
    Import("env")
    apply(Path(env.subst("$PROJECT_DIR")))
elif __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("prepared", type=Path)
    print(json.dumps(apply(parser.parse_args().prepared), indent=2))

#!/usr/bin/env python3
"""Apply reviewed rear-only safety fixes to the exact SparkFun BNO08x 1.0.6 sources.

The original Git blob IDs bind each edit to the reviewed upstream bytes.
The receipt binds repeat invocations to SHA-256 of all patched bytes.
"""
import hashlib
import json
from pathlib import Path

ORIGINAL = {
    "SparkFun_BNO08x_Arduino_Library.cpp": "3a5c6adc07551b755aea475e4fb02d41b8144376",
    "sh2.c": "d2f00f0b00d2d80efa78f10ac58e2500aa55fb4c",
    "shtp.c": "503b5eb5d3b001862987b90cbfad68648054543e",
}

def once(text, old, new):
    if text.count(old) != 1:
        raise ValueError("BNO dependency patch context changed: " + old[:80])
    return text.replace(old, new)

def apply(prepared):
    roots = list((Path(prepared) / ".pio/libdeps/esp32cam_s3_wroom_n16r8").glob(
        "*/src/SparkFun_BNO08x_Arduino_Library.cpp"))
    if len(roots) != 1:
        raise ValueError("Resolve the one pinned SparkFun BNO08x 1.0.6 dependency first")
    root = roots[0].parent
    receipt = root.parent / "rear-bno-safety-patch.json"
    if receipt.exists():
        saved = json.loads(receipt.read_text())
        if saved.get("schema") != 4:
            raise ValueError("Remove the old patched BNO dependency and reinstall pinned 1.0.6")
        if saved.get("original_git_blobs") != ORIGINAL:
            raise ValueError("Unexpected BNO dependency patch receipt")
        for name in ORIGINAL:
            if hashlib.sha256((root/name).read_bytes()).hexdigest() != saved["sha256"][name]:
                raise ValueError("Patched BNO dependency changed: " + name)
        return saved
    texts = {}
    for name, expected in ORIGINAL.items():
        data = (root/name).read_bytes()
        blob = hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()
        if blob != expected:
            raise ValueError("Unreviewed BNO dependency bytes: " + name)
        texts[name] = data.decode()
    texts["sh2.c"] = once(texts["sh2.c"],
        """((pOp->timeout_us == 0) ||
            ((now_us-start_us) < pOp->timeout_us))""",
        """((now_us-start_us) < (pOp->timeout_us ? pOp->timeout_us : 2000000U))""")
    texts["sh2.c"] = once(texts["sh2.c"],
        "    shtp_close(pSh2->pShtp);",
        "    if (pSh2->pShtp != 0) shtp_close(pSh2->pShtp);")
    texts["shtp.c"] = once(texts["shtp.c"],
        """        // Transmit (try repeatedly while HAL write returns 0)
        status = pShtp->pHal->write(pShtp->pHal, pShtp->outTransfer, lenField);
        while (status == 0)
        {
            shtp_service(pShtp);
            status = pShtp->pHal->write(pShtp->pHal, pShtp->outTransfer, lenField);
        }
""",
        """        // Rear optional sensor: a failed peer must not hold this worker forever.
        uint32_t writeStartUs = pShtp->pHal->getTimeUs(pShtp->pHal);
        status = pShtp->pHal->write(pShtp->pHal, pShtp->outTransfer, lenField);
        while (status == 0 &&
               (uint32_t)(pShtp->pHal->getTimeUs(pShtp->pHal)-writeStartUs) < 2000000U)
        {
            shtp_service(pShtp);
            status = pShtp->pHal->write(pShtp->pHal, pShtp->outTransfer, lenField);
        }
        if (status == 0) status = SH2_ERR_TIMEOUT;
""")
    texts["SparkFun_BNO08x_Arduino_Library.cpp"] = once(
        texts["SparkFun_BNO08x_Arduino_Library.cpp"],
        """	_spiPort->begin(); //Turn on SPI hardware""",
        """	// The rear worker already started SPI with the harness pin assignments.""")
    texts["SparkFun_BNO08x_Arduino_Library.cpp"] = once(
        texts["SparkFun_BNO08x_Arduino_Library.cpp"],
        """  hardwareReset();

  // Open SH2 interface""",
        """  if (_HAL.open != spihal_open) hardwareReset();

  // The rear worker owns the SPI reset and has already observed INT low.
  // Open SH2 interface""")
    texts["SparkFun_BNO08x_Arduino_Library.cpp"] = once(
        texts["SparkFun_BNO08x_Arduino_Library.cpp"],
        "bool BNO08x::_init(int32_t sensor_id) {",
        """static uint8_t rearSpiReply[SH2_HAL_MAX_TRANSFER_IN];
static unsigned rearSpiReplySize;
static uint32_t rearSpiReplyTime;
static int32_t rearSpiDiagnostic[10];

bool rearBnoSpiPending() { return rearSpiReplySize != 0; }

void rearBnoSpiDiagnostics(int32_t *out) {
  memcpy(out, rearSpiDiagnostic, sizeof(rearSpiDiagnostic));
}

bool BNO08x::_init(int32_t sensor_id) {""")
    texts["SparkFun_BNO08x_Arduino_Library.cpp"] = once(
        texts["SparkFun_BNO08x_Arduino_Library.cpp"],
        "  if (status != SH2_OK) {\n    return false;\n  }\n\n  // Check connection",
        "  rearSpiDiagnostic[0] = status;\n  if (status != SH2_OK) {\n    return false;\n  }\n\n  // Check connection")
    texts["SparkFun_BNO08x_Arduino_Library.cpp"] = once(
        texts["SparkFun_BNO08x_Arduino_Library.cpp"],
        "  status = sh2_getProdIds(&prodIds);",
        "  status = sh2_getProdIds(&prodIds);\n  rearSpiDiagnostic[1] = status;")
    texts["SparkFun_BNO08x_Arduino_Library.cpp"] = once(
        texts["SparkFun_BNO08x_Arduino_Library.cpp"],
        """  // Serial.println("SPI HAL open");

  hal_wait_for_int();

  return 0;""",
        """  memset(rearSpiDiagnostic, 0, sizeof(rearSpiDiagnostic));
  rearSpiDiagnostic[1] = -9999; // Product-ID request has not run.
  rearSpiReplySize = 0;
  return 0; // The application owns reset and attention detection.""")
    texts["SparkFun_BNO08x_Arduino_Library.cpp"] = once(
        texts["SparkFun_BNO08x_Arduino_Library.cpp"],
        """  // Serial.println("SPI HAL close");""",
        """  rearSpiReplySize = 0;""")
    # Report configuration passes through the bounded SPI HAL. The upstream
    # pre-wait can reset the chip while it is processing the previous command.
    name = "SparkFun_BNO08x_Arduino_Library.cpp"
    texts[name] = once(texts[name],
        "  config.reportInterval_us = interval_us;\n\n  if(_int_pin != -1)",
        "  config.reportInterval_us = interval_us;\n\n  if(_HAL.open != spihal_open && _int_pin != -1)")
    sparkfun = texts["SparkFun_BNO08x_Arduino_Library.cpp"]
    begin = sparkfun.index("  // Serial.println(\"SPI HAL read\");")
    end = sparkfun.index("\n}", begin)
    texts["SparkFun_BNO08x_Arduino_Library.cpp"] = sparkfun[:begin] + """  *t_us = micros();
  if (rearSpiReplySize != 0) {
    const unsigned size = rearSpiReplySize;
    rearSpiReplySize = 0;
    if (size > len) return 0;
    memcpy(pBuffer, rearSpiReply, size);
    *t_us = rearSpiReplyTime;
    return size;
  }
  if (len < 4 || digitalRead(_int_pin)) {
    delay(1);
    return 0;
  }

  // INT can deassert as soon as CS goes low. Keep CS asserted from header
  // through payload, as in CEVA's SH2 SPI reference transport.
  memset(pBuffer, 0, 4);
  _spiPort->beginTransaction(SPISettings(_spiPortSpeed, MSBFIRST, SPI_MODE3));
  digitalWrite(_cs, LOW);
  _spiPort->transfer(pBuffer, 4);
  const unsigned size = (pBuffer[0] | (unsigned(pBuffer[1]) << 8)) & 0x7fff;
  if (rearSpiDiagnostic[2] == 0) {
    uint32_t header;
    memcpy(&header, pBuffer, sizeof(header));
    rearSpiDiagnostic[8] = static_cast<int32_t>(header);
    rearSpiDiagnostic[9] = -1;
  }
  ++rearSpiDiagnostic[2]; // Receive transactions that saw INT low.
  rearSpiDiagnostic[3] = size;
  if (size > 4 && size <= len) {
    memset(pBuffer + 4, 0, size - 4);
    _spiPort->transfer(pBuffer + 4, size - 4);
    if (rearSpiDiagnostic[4] == 0) rearSpiDiagnostic[9] = pBuffer[4];
  }
  digitalWrite(_cs, HIGH);
  _spiPort->endTransaction();
  if (size > 4 && size <= len) ++rearSpiDiagnostic[4];
  return size > 4 && size <= len ? int(size) : 0;""" + sparkfun[end:]

    texts["SparkFun_BNO08x_Arduino_Library.cpp"] = once(
        texts["SparkFun_BNO08x_Arduino_Library.cpp"],
        """  if (!hal_wait_for_int()) {
    return 0;
  }

  spi_write(pBuffer, len);""",
        """  if (len < 4 || len > sizeof(rearSpiReply)) return -1;
  if (rearSpiReplySize != 0) return 0; // Service the buffered receive before the next write.
  if (digitalRead(_int_pin)) {
    ++rearSpiDiagnostic[5];
    delay(1);
    return 0; // SHTP retries with a deadline. Only the worker may reset the chip.
  }

  // SPI is full duplex. Preserve the packet shifted in while sending SH2
  // commands, including a reply whose body is longer than the command.
  memcpy(rearSpiReply, pBuffer, len);
  rearSpiReplyTime = micros();
  _spiPort->beginTransaction(SPISettings(_spiPortSpeed, MSBFIRST, SPI_MODE3));
  digitalWrite(_cs, LOW);
  _spiPort->transfer(rearSpiReply, 4);
  const unsigned received = (rearSpiReply[0] | (unsigned(rearSpiReply[1]) << 8)) & 0x7fff;
  const bool valid = received > 4 && received <= sizeof(rearSpiReply);
  const unsigned clocks = valid && received > len ? received : len;
  if (clocks > len) memset(rearSpiReply + len, 0, clocks - len);
  if (clocks > 4) _spiPort->transfer(rearSpiReply + 4, clocks - 4);
  digitalWrite(_cs, HIGH);
  _spiPort->endTransaction();
  ++rearSpiDiagnostic[6];
  if (valid) {
    ++rearSpiDiagnostic[7];
    rearSpiReplySize = received;
  }""")
    result = {"schema": 4, "library": "SparkFun BNO08x Cortex Based IMU@1.0.6",
              "original_git_blobs": ORIGINAL, "sha256": {}}
    for name, text in texts.items():
        data = text.encode()
        (root/name).write_bytes(data)
        result["sha256"][name] = hashlib.sha256(data).hexdigest()
    receipt.write_text(json.dumps(result, indent=2) + "\n")
    return result

if "Import" in globals():
    Import("env")
    apply(Path(env.subst("$PROJECT_DIR")))
elif __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("prepared", type=Path)
    print(json.dumps(apply(parser.parse_args().prepared), indent=2))

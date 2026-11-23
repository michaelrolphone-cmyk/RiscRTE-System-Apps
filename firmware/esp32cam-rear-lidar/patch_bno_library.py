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
        """  if (!hal_wait_for_int()) {
    return 0;
  }

  spi_write(pBuffer, len);""",
        """  if (!hal_wait_for_int()) {
    return -1; // Abort this transfer; SHTP must not retry a disconnected peer forever.
  }

  spi_write(pBuffer, len);""")
    texts["SparkFun_BNO08x_Arduino_Library.cpp"] = once(
        texts["SparkFun_BNO08x_Arduino_Library.cpp"],
        """  uint16_t packet_size = 0;

  if (!hal_wait_for_int())""",
        """  uint16_t packet_size = 0;
  *t_us = micros(); // Supply the host timestamp required by the SH2 HAL contract.

  if (!hal_wait_for_int())""")
    result = {"schema": 1, "library": "SparkFun BNO08x Cortex Based IMU@1.0.6",
              "original_git_blobs": ORIGINAL, "sha256": {}}
    for name, text in texts.items():
        data = text.encode()
        (root/name).write_bytes(data)
        result["sha256"][name] = hashlib.sha256(data).hexdigest()
    receipt.write_text(json.dumps(result, indent=2) + "\n")
    return result

if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("prepared", type=Path)
    print(json.dumps(apply(parser.parse_args().prepared), indent=2))

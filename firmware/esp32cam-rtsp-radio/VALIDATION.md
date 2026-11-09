# ESP32-S3 CAM validation

Only esp32cam_s3_wroom_n16r8 is a build target in this version.

## Native firmware

PASS on the Mac through BiomeRTE. PlatformIO 6.1.19.

Application binary: 1462864 bytes.

SHA-256: 313f98f500cefae14e915d43ec9a7b2742df6cfc16cb1aea3167eca019741003

## Host checks

- Receiver: 16 tests passed, including real localhost UDP delivery, malformed data, rotation, freshness, reordering and sequence wrap.
- C++ model: compile and tests passed for MAC validation, range math and signed iBeacon calibration.
- Dashboard JavaScript syntax passed in Node.
- git diff --check passed.

## Physical qualification

The ESP32-S3 CAM has not been flashed with this version. Live RF reception, FTM accuracy, OTA behavior, and camera/radio coexistence remain unmeasured. RSSI distance is explicitly approximate.

## S3 package reproduction

PASS: the preparation command reconstructed all 113 source files byte-for-byte. The resulting PlatformIO configuration contains exactly one S3 environment.

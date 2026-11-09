# Validation record

## Native firmware builds

| Environment | Result | Application bytes | SHA-256 |
| --- | --- | ---: | --- |
| esp32cam_s3_wroom_n16r8 | PASS | 1462864 | 313f98f500cefae14e915d43ec9a7b2742df6cfc16cb1aea3167eca019741003 |
| esp32cam_ai_thinker | PASS | 1619152 | 552a1de4ddc7dc34af3a1e227f6cf22ccf17a99a4f6561a9d5c38c34865c2b66 |
| esp32cam_espressif_esp32s2_cam_board | PASS | 1296512 | cb3405b98eefe55dbf79c1a6f5096f18c5d264982ca0f416ddffb0bf00dcccb0 |

All builds ran on the Mac via BiomeRTE. PlatformIO 6.1.19; upstream commit 14057dde6b2c81e7a1d06c6d7740a1b4f130b8b7. S2 initially tried compiling the Bluetooth-only dependency; the S2 environments now omit NimBLE because that chip has no Bluetooth controller. Its corrected build passed.

## Host checks

- Receiver: 16 tests passed, including localhost UDP delivery, malformed input, bounded rotation, freshness, reordering, and sequence wrap.
- C++ radio model: compile and tests passed for MAC validation, range math and signed iBeacon calibration.
- Dashboard JavaScript: Node syntax check passed.
- git diff --check: passed.

## Physical qualification

No board was flashed. Live radio discovery, FTM accuracy, OTA behavior, and RTSP/radio coexistence have not been measured. RSSI ranges remain explicitly approximate.

## Package reproduction

PASS: prepare.py cloned the pinned upstream and reconstructed 113 tracked/added source files byte-for-byte against the validated working tree.

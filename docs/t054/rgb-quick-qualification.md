# T054 RGB Quick Actions source checkpoint

Atomic Quick Actions now supports ordinary RGB brightness, audio volume, and mute/restore in the shared controller. Muting publishes alarm_volume and its subordinate quick_volume in one copied atomic group; the restore key cannot be an independent USER intention. Stale captured values, stale restore values, I/O refusal, and cancellation preserve the prior values. Optional frontlight-tone and radio code remains feature-selected. The complete RGB adapter compiles without paper/tone/resident-Home flags.

The attached Quick receipt records 49 ASan/UBSan cases (8 RGB cases plus the existing 41-case X4 matrix), the full RGB adapter compile command, and source/SDK hashes. The existing X4 Settings controller passes 47 current-source ASan/UBSan cases after these shared header changes. These tests use provider doubles, not hardware.

Reproduce Quick with scripts/test_quick_atomic.py --sdk <complete staged SDK include> --output <directory> --sanitize. The SDK must include the preserved X4 display-history/frontlight suffixes for the paper fixture, current Runtime atomic interfaces, and current System app headers. The receipt records all staged header hashes. Reproduce Settings with scripts/test_settings_atomic_x4_current.py --runtime <Runtime checkout> --output <fresh directory>.

Product selection remains off. Before enabling Waveshare namespace 1, finish the explicit Utilities and automatic-policy writers, qualify the exact expanded writer allowlist and native memory cost, rebuild every affected selected application without dropping cumulative features, and rerun product admission/update-image verification. No T054 firmware release or physical qualification is claimed by this checkpoint.

# Radio discovery without a full-sweep camera pause

Firmware `1.0-radio.13-radio-slices` retains passive Wi-Fi discovery, BLE, FTM, camera exposure, synchronized capture, AHRS, per-sonar capture history and front ECHO GPIO40.

The old passive all-channel Wi-Fi sweep coincided with a 2.28 s camera capture gap: its completion was 100 ms before the first subsequent frame. This revision keeps 120 ms dwell on every channel allowed by the current SDK country configuration, but starts channels at least 1000 ms apart. Sweep starts retain wifi_interval_ms cadence, subject to completing the prior sweep and preserving inter-channel spacing. BLE/FTM arbitration remains unchanged. A cancelled or failed start never fabricates observations.

Status exposes wifi_scan_mode, wifi_scan_result_scope, wifi_scan_channel (current/most recently started channel), wifi_scan_dwell_ms and wifi_channel_spacing_ms. wifi_last_scan_age_ms refers to the latest completed channel. Individual observations keep their actual last-seen ages and a 300 s TTL; a new channel result never refreshes unseen APs. A sweep is no longer a simultaneous batch. For short configured intervals, channel spacing sets the minimum effective sweep period. Live qualification must cover an entire sweep and verify radio observations continue while camera, AHRS and sonar remain available.

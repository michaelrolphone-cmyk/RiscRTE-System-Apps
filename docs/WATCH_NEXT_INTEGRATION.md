# Watch 1.0.5 integration source

This task branch combines the tested tap settings, power status and focused Springboard commits, preserving each source commit as a merge ancestor. SDR Watch 1.0.3/1.0.4 remains frozen separately.

The Watch-local tap policy changes linked sleep-client bytes, so existing unchanged-feature clients receive new versions: Wi-Fi Settings 1.1.4, Firmware Update and App Store 1.1.3, and portable File Browser 1.4.1. Settings is 1.3.0 and Springboard is 1.5.0. Provider versions remain unchanged because their source and linked profiles are unchanged.

The shared catalog default remains 17. SDR selects 18; the later HID-enabled cohort selects 20. Tests cover both historical profiles and the expanded bound, including oversized/rejected counts. No shared runtime hardware access or storage grants are introduced by the catalog change.

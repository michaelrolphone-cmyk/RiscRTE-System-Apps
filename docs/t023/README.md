# X4 foreground automatic provisioning

Preserves System09400a55 (.121 Home) and the complete sparse/RTC/sleep/Quick/Contexts/Broadcast profile. Automatic provisioning starts after a completed foreground Home frame, settled display, loaded radio policy and neutral gesture. Timer-only entry cannot start it. It pauses background Contexts/Broadcast and drains pending service/network cancellation before modal navigation, sleep and final teardown. Existing supplied UTC reconciliation is retained.

62 production Clock/adapter lifecycle cases pass normal and Linux ASan/UBSan, including positive first-frame/radio/display/gesture gates and cooperative pending cancellation at fini. The strict fixture models availability and optional atomic absence; atomic transaction behavior remains covered by its dedicated suites. macOS target compilation reaches the previously reproduced Xtensa BFD linker assertion; Linux target reproduction is required before product integration. No hardware qualification.

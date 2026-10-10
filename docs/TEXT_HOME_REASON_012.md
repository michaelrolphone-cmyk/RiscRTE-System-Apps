# Shared text 0.1.2 recovery checkpoint

Recovered System8df9291 text0.1.1, adding an optional negotiated physical-Home cancellation reason for Points0.6.13. Back/Escape remain plain CANCELLED. Old request/result layouts and API callback prefix are unchanged. The tagged suffix advertises support; only opted-in requests may receive HOME_CANCEL.

Logical cancellation is immediate. Clean close still waits for any actually accepted display frame and releases all retained custody before returning to the caller. Same-poll Accept/Back before Home does not mislabel the terminal result. Copied text, revision exhaustion, stale handle rejection and fail-closed retention remain covered.

Qualification completed:
- Host/client normal and ASan/UBSan, including exact old-prefix allocation, malformed suffix, old client flags, pending close and invalid Home states
- 512 real Runtime0.1.100 scenarios: X4 and Watch, normal and ASan/UBSan, four provider activation policies
- 256 real Runtime0.1.106 X4 scenarios, normal and ASan/UBSan, against frozen recovered scene0.1.4 hostabb5025
- Clean Xtensa GCC8.4 target compilation and strict loader validation

The old fast-keyboard test was updated to the approved scene0.1.4 semantics: owner-acknowledged logical layer changes accept input immediately while the display remains pending. The pending-close fixture now waits for an actual accepted frame rather than confusing queued paint with physical custody.

No product binary is a build input. Rebuild with scripts/build_text_home_service.py after final source recovery. This isolated checkpoint does not replace the final reconciled System union: its portable-text attention change is owned by that union and requires a combined regression run. Hardware and full-product qualification remain separate.

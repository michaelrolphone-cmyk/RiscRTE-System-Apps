# Master parity refresh — 2026-10-01

Reader master: `1e0188c1ff0234dd33fe054c9a6fb4fde36596df`.
Published release-index: `572746f4fcf3fde19947a066b7e5c8028cd76d21`.
Fresh target/source tree audits preceded writes; no external source conflict was found.
Independent build tooling and pinned SDK were retained. Published versions were copied,
without inventing a further version bump for byte-identical released payloads.

Target base: `b64e1c99371946b7f7ff19a0ec0587859beb584b`.
Only Button Remap, Clear Cache and OTA Update C/manifest pairs drifted.
Their local blobs matched the recorded bases (upstream-only changes).
Each app moves 1.0.0 → 1.0.1. Button Remap labels side controls explicitly;
Clear Cache and OTA accept semantic Confirm only, not body/header taps.

Validation: all 38 app source/manifest/helper inputs match the pinned master;
10 pipeline tests, 18 host fixtures and 18 ELF/import/structure/sidecar checks pass.
All eight required published-byte comparisons pass, including the three refreshed apps:

| App | Bytes | SHA-256 |
|---|---:|---|
| button_remap 1.0.1 | 3812 | `46ff711c7a3cf9ca5ed2bab41088b57e0821dde6c5be0e3abb891c6a5f17f154` |
| clear_cache 1.0.1 | 3560 | `8d2eaa8ea68bae515fd2b51ce4553ba723d58c8fd838a5ded6c125dee1d77b30` |
| ota_update 1.0.1 | 4060 | `159845f5504f74cc60a3464f34968f07b098b916cebd678ee99373fe882d04d5` |

The ten unchanged historical development-build/release-byte differences remain
visible in `release-parity.json`; source parity does not erase those differences.
Image Viewer has no host UI fixture. Shared firmware touch/orientation dispatch
and actual hardware are outside this external host validation.

CI must pass at the final PR head before ready/merge; run evidence is recorded in
the PR and maintenance claim #5. Independent publication, ZIP/catalog integration,
prospective U1 compatibility and runtime cutover remain unqualified;
`parity_ready_count` remains zero. Other external repos were not refreshed here.

The unchanged File Browser sanitizer regression timed out locally on macOS at its existing 10-second limit. Linux CI retains the original sanitizer gate; it must pass before merge.

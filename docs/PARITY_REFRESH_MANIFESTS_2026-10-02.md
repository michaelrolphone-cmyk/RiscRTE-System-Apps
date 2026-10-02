# Published manifest reconciliation — 2026-10-02

- Reader master snapshot: `82caa0997e913f01c1f5f9ab942d056bc9f04a82`.
- System-Apps starting main: `ea43e8f4da5bef475e8d12454dc465767a947a61` (PR #14 already merged; no repeat).
- Current source audit: all three affected C files and the usb_ui_navigation C/manifest pair match Reader; only these three app manifest versions remained behind.
- Button Remap, Clear Reading Cache, Firmware Update changed from target manifest 1.0.1 to current Reader and already-published 1.0.2. App source, SDK pins, compiler, package workflow, and driver payload were unchanged.
- Reader release workflow artifact `11209466823` from successful run `36965130240` on source commit `f7f006f78bf1f83c28f3ce05728b8973e895956b` supplied the exact three `.rte.zip` files. Their sizes and SHA-256 values match GitHub release asset metadata:
  - `button_remap` 1.0.2: ZIP 4881 bytes / `34900287855c4ae0f14d61defb99bca05dc539fe9474fedb8eae5aea252135ab`; embedded ELF 3812 bytes / `46ff711c7a3cf9ca5ed2bab41088b57e0821dde6c5be0e3abb891c6a5f17f154`.
  - `clear_cache` 1.0.2: ZIP 4624 bytes / `7456674bf42fcb73709a65cd648ec741f4db64181920f72d82533089adf872b1`; embedded ELF 3560 bytes / `8d2eaa8ea68bae515fd2b51ce4553ba723d58c8fd838a5ded6c125dee1d77b30`.
  - `ota_update` 1.0.2: ZIP 5108 bytes / `ed71c3f7d2de88f48e0758e0000b03cbb35d86e75b91673b3fb2f7a8c9ce31de`; embedded ELF 4060 bytes / `159845f5504f74cc60a3464f34968f07b098b916cebd678ee99373fe882d04d5`.

The embedded ELF identities equal their existing 1.0.1 releases, so this was a released metadata sync only; no new bump or publication is needed. The full independent external app pipeline and all three focused builds are expected to validate on the PR head. CI remains the exact-head qualification.

Source-drift accounting: the three manifest rows converge to current Reader. Twelve other manifest-only rows remain upstream-only and untouched. `parity_ready_count` remains 0; this does not establish U1 ZIP/runtime, install, device, physical, or cutover readiness. No Reader/U1 writes, Driver/X4 or hardware work, releases, live catalog edits, deployments, security changes, or runtime switch are included.

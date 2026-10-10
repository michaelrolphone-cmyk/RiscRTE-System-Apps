# Selected X4 Home desk lock

The selected `--desk-lock-home` profile removes the Home wordmark and turns the
Home/back navigation key into an explicit deep desk-clock request. The adapter
waits for key release, completes an outstanding presentation, and abandons any
unsubmitted Home frame before transferring control to the existing typed X4
sleep hook. A retained result fences all later activity. A clean refusal or
cancellation restores the foreground orientation and neutral input state.

The desk direction is the namespace-1 `desk_direction` preference, independent
of `reader_flip_ui`. Its two values render the 800×480 native surface at 0° and
180°. Invalid or unavailable direction bytes refuse the lock while leaving
Home usable. A timer wake uses only the retained scene and direction; GPIO wake
enters ordinary foreground Home. Runtime still owns record integrity and
app/cohort binding, raw wake authority and terminal admission.

Desk frames use CLEAN for the initial/periodic full update and QUALITY for
minute damage. Foreground motion continues to use LOW_LATENCY. The selected
display provider must support the normal-quality waveform and previous-image
seeding; the application fixture does not qualify a provider's physical output.

## Actual-source regression

`scripts/test_home_desk_lock.py` verifies the target ELF digest and every
production source/SDK hash in its build receipt, then compiles the same defines,
Clock, adapter and product sleep hook against strict host provider doubles.
Normal and ASan/UBSan runs cover:

- Both landscape directions, all six faces, and independent flipped Home.
- Thirty-one successive fresh-process minute wakes per direction, seeded
  physical-image equality, QUALITY damage bounds and periodic CLEAN refresh.
- Same-minute timer wake with no unnecessary display submission.
- GPIO wake with the wake key held, initial held-key suppression, a 2.2-second
  locking hold and an outstanding asynchronous Home frame.
- Repress during entry, cancellation during painting, raw key refusal, native
  deep refusal, retained deep custody and uncertain display submission.
- A stored LIGHT preference overridden by explicit Home lock; missing, corrupt,
  out-of-range and unavailable desk-direction records.
- Pixel equality of both direction variants under 180° rotation, and the
  original Home dial/header after cancellation/refusal.

The selected run executes 220 cases. Generated evidence, logs, native pixels
and PNGs are under `build/home-desk-lock-tests/`. The inspected Home capture has
no NOVA-7 text; the landscape faces and restored Home remain within bounds.
ASan leak detection is disabled because the sandbox does not support LSan;
address/undefined-behavior checking remains enabled with fatal errors.

Example invocation after building the selected target:

```sh
python3 scripts/test_home_desk_lock.py \
  --candidate build/desk-lock-qualified \
  --x4 ../x4-home-desk-lock-product \
  --runtime ../x4-runtime-startup-0162
```

The target is compiled against the canonical Drivers 0.1.4 SDK, Runtime
`c546dae` (0.1.62) app/driver SDK, and immutable Utilities alarm 2/Points headers
at `637e13b0bce62ad49b756bec2468a6271d163fc7`. The product sleep hook is
`962aec2`. Target ELF structural validation and import/export checks pass.

These are app/controller host and target-build checks. Physical key wake,
panel cleanliness/ghosting, power/current consumption and the complete
Runtime/provider graph still require their separate integration or hardware
qualification. No product metadata, BIN, device, or external publication is
changed by this work.

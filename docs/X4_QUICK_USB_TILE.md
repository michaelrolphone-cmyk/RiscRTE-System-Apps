# X4 shared Quick Controls: USB tile correction

This focused source unit follows delivered X4 0.1.51 and System source
`c336b776c869029ecd9f096aa5e6497759581440`. Its selected resident-policy Home
package is `paper_clock@0.3.23`. It is not a new composed product image.

## Behavior

- The one resident host continues to own Quick Controls above Home and hosted
  foreground apps. No client-local renderer or gesture handler is introduced.
- USB Transfer uses the same 204 × 92 logical grid cell and rounded rendering
  as the other controls, with or without sound hardware.
- With no sound output, Volume and Silent remain absent. The grid contains
  DND/Airplane, Wi-Fi/Bluetooth, Frontlight/USB Transfer, then Clean Refresh.
  Clean Refresh remains conditional on the display capability.
- The renderer and input controller use the same tile-position function. The
  old full-width USB row and its independent hitbox are removed.
- USB activation retains the existing app transition. The separate transfer
  screen still owns its explicit Start/Stop operations and USB/SD custody.
  A tile tap does not silently start a transfer or claim an active USB state.
- Ordinary overlay presentation remains low-latency. Only explicit Clean
  Refresh selects the clean waveform. No display-driver code changes.

Brightness, frontlight on/off and capability-selected audio suppression already
exist in the baseline. Warm/cool tone is not implemented by this unit: the pinned
frontlight provider exposes a single level and drives its cool and warm channels
equally. A real tone control needs a separately versioned provider/SDK contract,
safe channel handling, persistence, host controls, and qualification.

## Verification

- `scripts/test_shared_quick_tiles.py`: audio/no-audio and USB-selected/unselected
  geometry, input cancellation and 100 repeated USB interactions, normal and
  ASan/UBSan configurations.
- `scripts/test_shared_quick_reference.py`: 40 production Runtime/Graph + actual
  host/client ELF executions, normal and ASan/UBSan. Covers Home/child routing,
  repeated USB entry and return, cancellation, obsolete-row rejection,
  brightness persistence, audio availability, explicit clean refresh, and
  retained display failures. Non-clean overlay submits assert low-latency intent.
- Selected target Home compiles and passes ELF structural/import/export checks.
- Current Home gesture (300), crown/drawer (8) and idle (36) regression cases,
  the 108 shared Quick Actions executions, and shared text-host tests are kept
  separate from hardware qualification.

The old app-local `test_quick_usb_transfer.py` and `test_paper_quick_actions.py`
fail their pre-existing waveform assertions on both this unit and the exact
unmodified baseline. They do not establish a new regression or an all-suite pass.
No physical panel, USB device, network exposure, publication, or delivered-binary
modification is performed by this source unit.

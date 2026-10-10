# Universal UI decoupling: second incremental checkpoint

Starts from delivered X4 .55 System `e171fd6`. No display mode, feature,
control, gesture, layout, animation, public API or hardware capability is removed.

## New fixes

- App Store and Firmware Update snapshot current logical geometry and exact
  release identity at DOWN. Confirm operates on current selection and revalidates
  the release. Display snapshots remain diagnostic/raster history only.
- Shared broadcast and Contexts state steps advance during immutable in-flight
  presentation. Capture/storage/radio exclusion and terminal custody fences remain.
- Home Contexts interaction eligibility is separate from display settling. Only
  the model-owner handoff waits for display completion; actual sleep still uses
  the original settled-display requirement.
- Low-battery policy samples and applies a threshold crossing independently of
  a submitted display image. A mutable app raster lease remains excluded.

The independent Contexts app/detector implementation is unchanged. Shared text,
scene and keyboard behavior is unchanged. Synchronous legacy display providers
retain their existing bounded-wait fallback; this source alone does not claim
that an unconverted provider is nonblocking. The selected Watch LCD, X4 fast
UC8279 and normal UC8279 already expose bounded asynchronous owner-task progress.
SSD1677 provider conversion and qualification are a separate checkpoint.

## Qualification

- 320 actual updater/controller cases: App Store and Firmware Update; ordinary
  and flipped orientation; 0/17/2300 ms and never-completing displays; normal and
  ASan/UBSan. New state is verified before the first slow image completes.
- Rapid navigation and taps, list scrolling, exact-release selection, explicit
  install confirmation, cancellation and catalog replacement/version/removal
  during contact. Replacing a release after confirmation still blocks install.
- Stable final screens have identical raster hashes at 0/17/2300 ms. Submitted
  pixels remain immutable. Never-completing displays retain custody at the
  existing deadline without losing preceding logical actions.
- 130 actual shared-adapter Contexts checks, including broadcast-enabled and
  editor profiles, normal and sanitized; new busy-state case proves service
  progress while display completion remains unavailable.
- 72 Contexts owner-rendezvous tests preserve export, grant and terminal rules.
- 162 Settings/Time Zone/Wi-Fi/Files/Springboard logical-scroll cases pass.
- 16 RGB565/MONO1 frame-delivery cases pass with 60 touch cycles and 60 navigation
  press/release cycles per case; latest-state presentation and custody retained.
- Delivered GameBoy `a579dedd` passes 14 fresh normal/sanitized timing/mailbox
  cases. Fast/LCD/slow presentations retain identical logical histories and
  final CPU/APU states (597 frames; compute-heavy profiles 323 frames).

Receipts are under `docs/qualification/`. Results use deterministic host I/O
providers and are not hardware timing claims. The historical nested idle
Settings fixture fails on the unchanged baseline as well as this candidate
because it does not emit the ordered input stream; it is not counted as passing.

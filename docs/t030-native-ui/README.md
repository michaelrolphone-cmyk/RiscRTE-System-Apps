# Shared native Springboard and launch repair

This source is the shared application component for Waveshare 0.1.44 and T-Watch 1.1.35 test candidates. Physical-device performance and the reported intermittent blackout remain unaccepted until hardware testing.

## Implemented

- Springboard honeycomb layout and touch consume the display capability's physical dimensions. Its original Orbitron, Rajdhani and Font Awesome outlines are rasterized directly into the native framebuffer; there is no completed-view scaling.
- Input waits include elapsed provider work in their existing bounded budget. The same ordered input reducer serves native coordinates, fallback navigation, replay and Quick Controls.
- Raster capture checkpoints are grouped every four glyph rows. Independent full-feature replay proves identical pixels/motion with 46.9% fewer Contexts calls at 410×502.
- A newly opened brightness-capable display restores the current Quick brightness or torch level exactly once, after the first completed frame. Rejected/failed frames cannot unblank early. This repairs a reproduced dark-open path; it does not prove that every reported blackout was a crash or the same cause.
- Optional bounded stage telemetry reports rendering, input polling, catalog refresh, submit-to-observed-complete and input-edge age. Build with PORTABLE_STAGE_PROFILE. No new runtime ABI is introduced.
- Catalog diagnostics and the prior acquisition repair remain present. Runtime 54461e8a7f61b28600b8ec8dcaf27b12cd45e829 includes that repair plus selectable bounded trace storage. Product recipes explicitly select 64 records without changing the native IQ reservation.

## Product integration

Both watches use this same System source. Their deployment recipes retain all original feature flags, native provider profiles and storage layouts. Waveshare also adds the missing Home runtime.retained-wake@1 declaration and instance-0 boot grant. The verified T-Watch 1.1.33 baseline already has those grants, so its policy remains unchanged. Product repositories own exact compiled-artifact hashes, whole-store admission, preserving-kit power-cut tests and release receipts.

No e-ink release is implied by an RGB System pin. The paper-clock product has a separate System source closure; common framework applicability and integration are recorded in the shared-target qualification rather than silently replacing its product source.

## Evidence and limits

- renderer/ contains original-font provenance, exact tests, screenshots, native geometry coverage and repeated host measurements. qualification/ contains independent bounded vector/custody evidence.
- input-cadence/ contains the controlled provider model. A host or modeled duration is not physical display latency or device FPS. The existing 40 ms controller interval remains.
- The full native-height adapter is qualified at seven square/portrait sizes from 160×160 through 800×1000. Standalone renderer tests at 1..2048 and landscape sizes do not qualify all application controls at those sizes.
- Legacy Quick Controls, generic fallback text/control geometry, Home and other applications still require native migration. The coverage/ inventory records all 27 delivered application manifests and the remaining work. Those requirements remain open.
- Factory images erase internal data. Only an exact-baseline preserving kit with a fresh full-device readback can make a preservation claim; never substitute a full factory image for that process.

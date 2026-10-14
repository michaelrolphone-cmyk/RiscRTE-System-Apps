# Restored Springboard raster costs

This change descends directly from 72f435fcee376ec62d17ece6180753385ea547d9, the shared honeycomb delivered in Waveshare 0.1.26/0.1.27. No controller, motion, catalog, fonts, captions, gesture, feature switches, or provider ownership behavior changes.

## Trace

FT3168 reads one coherent 13-byte report per touch poll. The adapter consumes it after the cooperative poll wait, translates through the capability-derived WatchLayout, and passes it to nova_contact_input. The controller applies drag deltas and physics and requests dirty frames no faster than 40 ms. Rendering clears a native RGB565 canvas, draws icons/circles, applies the edge mask, then adds captions. At 410x502 the mask visits 205,820 pixels. The asynchronous canvas seals the completed image, copies it into a provider lease when available, and keeps only the latest pending frame. The panel sends paired rows from bounded provider polls. Runtime yield polls providers with a separate budget: its 1 ms requested sleep is not the display poll budget.

## Changes and limits

The scaled logical pixel path formerly revalidated generic geometry and performed 64-bit rectangle math on each pixel, then blended even fully opaque pixels. It now uses bounded 32-bit boundaries and an opaque fast path. Layout validation remains at acquisition. Circle square roots now run only at the antialiased rim. The full native mask hoists vertical geometry and uses exact quotient/remainder stepping horizontally, eliminating per-pixel coordinate division. No buffers or hardware-specific branches are added.

`test_springboard_raster_cost.py` compares every pixel and padded-row guard against frozen original functions at 240x240, 410x502, 320x390, 239x293, 128x128, and 1x2. It covers alpha 0..256, clipped/scaled pixels, radii 1..64, mask, and composed scenes under normal, ASan and UBSan builds. Its host benchmark compares circle/mask costs with the same pixel backend in both variants; it is conservative and does not measure the separately verified scaled-pixel optimization. Hardware touch-to-photon timing still requires the watch.

Existing Springboard motion, gesture/launch, caption/focus, capability-derived geometry and feature-profile tests must pass. No claim is made that host timing proves a particular on-device frame rate.

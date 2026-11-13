# Explicit native Quick presentation

`PORTABLE_QUICK_SAFE_CONTENT` selects provider-declared content bounds; call
`pqa_set_dimensions` then `pqa_set_safe_content(left,top,width,height)` while
closed/idle. Content minimum is 240×240, full surface maximum2048. Invalid
bounds fail rather than inventing board geometry. Render and hit rectangles
remain in full native coordinates; the opening gesture stays on the physical
screen top edge with full-screen thresholds. No final framebuffer scaling.
The existing native glyph/icon-mask renderer is retained (individual masks are
sampled); this is not the daily apps' original-font-face implementation.

`PORTABLE_QUICK_CAPABILITY_SAFE` independently gates Wi-Fi and Bluetooth.
Availability means an acquired provider; validity additionally requires checked
policy/status. Airplane needs both providers, never infers absent hardware Off.
Wi-Fi On is policy permission, not proof of association. Contexts requires a
real acquired service as well as readable preference; no background service is
created by a preference. Selected visual-only alarm Home leaves audio_available
false and labels volume as a preference only. Silent/volume do not create sound.

These macros are explicit source ABI choices: every linked TU using pqa_state
must select the same flags. Omitted profiles preserve their historical branch.
The copied preference store/controller contracts are unchanged: no raw writer
fallback, partial-group atomicity claim or uncertainty auto-confirmation.

Owning checks:
- scripts/test_quick_safe_content.py --output DIR: normal +ASan/UBSan;
  257 content sizes/offsets, full-edge opening/replay, cancellation/ID replacement,
  capability-specific hits, full/sliced rendering, guard/stride and actual pixel
  containment, six production raster states at466/69-insets.
- scripts/test_quick_native.py: omitted-profile legacy native/240 raster parity.
- Watch scripts/test_quick_capability_radios.py and test_generic_preferences.py
  --native-quick-actions: provider/controller qualification.

Rasters are modeled production renderer outputs, not device photographs.
Providers and physical radio/audio/sleep behavior remain device-unverified.

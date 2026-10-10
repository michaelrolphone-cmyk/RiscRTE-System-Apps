# X4 0.1.68 Home reuse

Hardware feedback for .67 reports responsive Springboard and Quick Actions but
continued slow Home. The Home controller used one `clock_dirty` flag for both
requesting presentation and invalidating its cached pixels. Every resident child
return therefore repainted the entire scene even when it was unchanged.

Home 0.4.5 keeps presentation demand separate from cache invalidation. A zeroed,
field-based key contains the displayed date/time (without seconds), battery,
time format, notice, press feedback, and rendered Points labels/times/durations
and progress width. Reconciliation generations and off-screen fourth entries do
not invalidate it. Layer validity still checks dimensions and orientation.
Unavailable caches still use immediate rendering, and pending frame readiness
keeps the dirty request until it can be presented.

The actual Home draw function is checked against independently painted complete
buffers for repeated return-style redraws and every visible key field. Warm
unchanged requests issue no paint spans. Normal and ASan/UBSan runs cover
orientation, pending readiness and allocation fallback. Actual adapter/Home
transition endpoint tests remain unchanged.

This targets repeated Home painting. It does not establish that cold Home load,
background service work or physical panel settling are fast on hardware. The
other .67 executables and panel 0.1.16 are reused byte-for-byte. Latest published
Contexts 0.4.0 and scene-host 0.3.0 are already in .67 and remain selected.

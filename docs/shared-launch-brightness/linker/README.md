# Qualified low-memory link recipe

The pinned GCC 8.4.0 / binutils 2.35.1 linker aborted on all six bounded attempts
for the newly changed Waveshare Waterfall image. The exact diagnostic was:
`collect2: fatal error: ld terminated with signal 6 [Aborted]`,
`compilation terminated.`, `double free or corruption (out)`.

The independent paired experiment changed only `-Wl,--no-keep-memory` between
commands using the same inputs and output path. Five of six original links
aborted; one succeeded. All six lower-memory links succeeded. All seven
successful outputs produce the same final loader-metadata-v3 ELF:
`5543af60434887b0d3b63ba0560c6ccc3445cfe4bd9713d154bbb7897e621b5c`.
The original six-attempt failure and all paired outcomes are retained here.

The same option is used for both watch Waterfall recipes. A fresh T-Watch
build remains byte-identical to its successful original-flags brightness
candidate. A fresh Waveshare build also passes. Full strict replays validate
all final hashes, sizes and symbol inventories separately.

This is a measured build mitigation, not an established allocator root cause
or a guarantee that binutils cannot fail again. Existing bounded retries
remain unchanged and still accept only the exact known signature. No app
features, grants, compiler optimization level, LTO partitioning or driver
policy changed. The pinned linker's own help describes this option as using
less memory and more disk I/O.

An alternative LTO-partition-none probe also linked six times, but changed
output bytes and was not selected. The matrix initially retained v4 debug
artifacts; finalization uses the unchanged production v3 compactor, verifying
that every original/lower-memory success matches the shipped recipe profile.

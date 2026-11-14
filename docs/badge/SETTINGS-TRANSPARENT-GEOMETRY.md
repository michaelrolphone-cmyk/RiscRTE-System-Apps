# Settings transparent-geometry candidate (not selected)

Base: System5fb53d9. Prepared fitting from5d9c7e0 is retained independently as
PORTABLE_SETTINGS_PREPARED_TEXT. This candidate introduces the separate opt-in
PORTABLE_SETTINGS_ALPHA_GEOMETRY; no builder/profile enables either switch.

The selected466 round root redraw traverses transparent native fade interiors.
Zero coverage cannot read/write pixels but previously repeated logical mapping,
native-side selection and bounds checks. The new path:

* Rejects zero coverage after the original scalar pixel increment/checkpoint,
  before pure coordinate/bounds work.
* Aggregates the exact no-write footprint of a fully transparent logical row
  only during raster_replaying && raster_input_slice. Adjacent mapped cell edges
  telescope to the same count. Every512-pixel checkpoint and unsigned-counter
  residue is retained, including sibling pixels outside the current replay band.
* Leaves callback-enabled/immediate rendering scalar. PORTABLE_FILE_SETUP is
  excluded from aggregation because its checkpoint may run service work before
  the input-slice fast return. Invalid/changed format, rotation, surface and
  geometry cannot authorize a write in this no-write path.

This differs from8937a35's row writer, which constructed row metadata inside every
logical pixel cell, and from6d2bd28's checkpoint batching. It does not reduce input
acquisition frequency, merge commands, change alpha/glyphs, or alter visible maps.

Selected-native/model/render flags with provider doubles,25 completed root scroll
frames at offsets0,5,...,120: native pixel calls9,341,461→1,302,961; native-side
calls28,669,120→3,657,012. The8,038,500 removed pixel calls were all transparent
fade samples. All25 frame hashes and displayed offsets match; budget22,367,487,
checkpoint count45,161 and checkpoint-counter hash17573339874704728739 match.
Only foreground-idle service flags are omitted in the host fixture; their exact
selected target configuration is checked by the separate target build.

Uninstrumented host-Os,10 alternating-order runs each: baseline median295.67ms,
prepared-fit293.34ms, transparent-geometry63.20ms, both65.68ms for all25 frames.
These are host CPU measurements, not device milliseconds or FPS. The candidate
has substantial host evidence; prepared fitting remains separately assessed.

Owning `test_settings_alpha_geometry.py --runtime-sdk SDK --output DIR` compares
baseline/alpha/both normal and ASan/UBSan:72 cases spanning compact240/native328/
466 geometry, MONO1/RGB565/invalid format, rotation/flip, round/non-round, replay
bands, callback-enabled and callback-free paths, same-pointer geometry change,
new pointer, format change, unbound surface and terminal failure. Complete pixels,
unsigned budget residue, checkpoint count/hash and callback count must match.
A failed checkpoint forbids further writes/callbacks. Native touch-scroll clip
flags require a different profile and are not enabled in this selected compact
Settings fixture; physical replay band clipping is covered.

Evidence: sibling badge-settings-whole-cost, with original failed fixture
configuration attempts retained. No publication or device execution occurred.

## Owning selection and target admission

`build_portable_settings.py` accepts independent `--settings-alpha-geometry` and
`--settings-prepared-text`. Each requires `--settings-profile native-time-touch
--native-settings-ui --sliced-native-frames` without paper transitions. Neither
is default. `settings_replay_geometry` in the build record identifies both
booleans, callback scope, preserved budgets/maps and no physical acceptance.
`test_settings_geometry_build.py` proves four independent combinations, unchanged
defaults and eight rejected combinations before any SDK/compiler work.

Exact selected5fb target recipe, compiler15.2.0-Os: disabled switches reproduce
selected debug/build-record SHA69bc864a013a342324aeb8d914c39f00c1855ee9edd5cfbd39d847c48f3a2a0a.
Alpha costs136 text bytes and no data/BSS; fitting costs416text/32BSS; both
cost528text/32BSS. Imports match all four ways. Selected sv_native_x calls
sv_native_side and sv_native_edge; edge calls sv_native_side again and performs
mull/mulsh for exact /240 mapping. The measured transparent-row reduction removes
8,365,000 such mapping calls and25,012,108 native-side calls over25 frames.
Each selected native-side call itself invokes width+height; their unrotated
RGB565466 paths total28 target instructions including the side function. This
supports a700,339,024-instruction avoided-work estimate for that nested subset
alone, not measured instructions/cycles, net total instructions, or device FPS.
New branch/run-accounting instructions and memory behavior must not be ignored.

Both-switch existing nine-page/sliced-map/OOM/15retained owning suites pass normal
and ASan/UBSan using the established acquisition-timestamp fixture correction.
The separate dynamic primitive suite compares alpha-only and both independently.

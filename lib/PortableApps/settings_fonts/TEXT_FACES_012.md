# Optional NOVA faces 0/1/2 profile

`PORTABLE_NOVA_TEXT_FACES_012` is an explicit product build selection. It removes
unused font records from the NOVA UI without changing glyph bytes, metrics,
placement, alpha, clipping, or scale. Generic builds still include `text.inc`
and retain all six original faces. The original generated file and OFL notices
are unchanged.

The selected table contains all 95 printable ASCII glyphs for each of:

- Face 0: original Orbitron, 14 px
- Face 1: original Rajdhani, 15 px
- Face 2: original Rajdhani, 12 px

`NOVA_FACE_ORBITRON_10` (face 6) uses original face 0 at the existing 183/256
renderer scale. Existing measurement and rendering behavior for this alias is
preserved. As before, the wrapping API does not accept face 6.

The selected measurement API returns zero for omitted faces 3/4/5; text,
centered text, and right-aligned text produce no pixels; wrapping returns false.
All table lookups are bounded before indexing. The subset must not be selected
for a product that needs omitted faces. Existing Settings, Wi-Fi, and Updates
full-face views produce a compile-time error if combined with this selection.
No app or product opts in implicitly.

## Reproduction and validation

`python3 scripts/generate_nova_text_faces_012.py` copies the original exact C
bitmap and metric initializers. It never loads a font or rerasterizes artwork.
The generator checks the full original file's pinned SHA-256 and all 570 ordered
original glyph names before selecting the 285 records. Its `--check` mode verifies
both the generated file and provenance without writing.

`python3 scripts/test_nova_text_faces_012.py` checks every initializer and then
compiles the real portable adapter in generic and subset profiles, each with and
without ASan/UBSan. It checks all 95 characters of faces 0/1/2/6 against the
untouched generic font, including exact RGB565 alpha blending on nonzero
backgrounds, signed bearings, partial clipping, screen edges, width rejection,
individual and 96-character metrics, invalid-character fallback, and rejected
faces. It also compares multi-glyph layout and alarm/LoRa-style widget frames
byte-for-byte between generic and subset builds.

On environments whose process tracer prevents LeakSanitizer from running, set
`ASAN_OPTIONS=detect_leaks=0` for the test process; address and undefined-behavior
sanitizers remain enabled. There is no heap allocation in the selected font code.

Keep `SOURCES.json`, `text-faces-012-source.json`, `LICENSE-Orbitron.txt`, and
`LICENSE-Rajdhani.txt` with the generated rasters. These fonts are SIL OFL 1.1.

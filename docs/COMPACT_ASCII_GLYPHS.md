# Generic compact ASCII punctuation

The original bounded generic `draw_text`/`draw_label` raster supports all32
printable ASCII punctuation characters through a shared5x7 column table.
This repairs invisible `+` controls and other punctuation such as `/` without
an app-specific label workaround or new font/provider dependency.

Existing digits, uppercase/case-folded letters and `-`, `:`, `%` columns are
unchanged. Space, control bytes and bytes outside printable ASCII remain blank;
this is not Unicode text support. Existing128-character bounds, clipping,
stride and raster snapshot/replay behavior remain in force. The new geometric
punctuation bitmaps are original source, not imported font assets.

`test_compact_ascii_glyphs.py` executes the actual renderer in RGB565 and MONO1,
normal and ASan/UBSan: exact `+` geometry for text and centered labels, all
punctuation nonempty, frozen9c2a049 legacy raster parity at clipped edges,
blank/length handling and stride/allocation guards. Software raster checks do
not establish physical panel visibility or touch accuracy.

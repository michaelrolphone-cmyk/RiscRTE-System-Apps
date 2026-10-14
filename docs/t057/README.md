# Optional scene landscape sessions

Telemetry table version 2 advertises the LANDSCAPE component-document flag.
The existing ABI layout and table prefix are unchanged, and clients accepting
version 1 remain compatible through the updated checked getter. A landscape
request rotates portrait logical geometry at session open, composing a 90-degree
raster rotation and the inverse touch transform with the selected profile.
Already-landscape geometry is unchanged. Profile objects are never modified.

Orientation cannot change through update/present on an active session. Clients
must close/reopen after settled frames and input. This keeps the immutable frame
and hit-map guarantees intact. Closing and reopening a normal document restores
the profile orientation. Other applications retain their previous rendering.

Run scripts/test_scene_telemetry.py --runtime RUNTIME for existing telemetry and
four-rotation landscape tests, normally and with ASan/UBSan. Run
scripts/test_scene_decoupling.py --runtime RUNTIME --baseline BASELINE --output OUT
for unchanged raster/cadence checks. Use the committed telemetry glyphs for the X4 build. They are recovered from
the qualified .114 ELF by scripts/recover_telemetry_fonts.py (SHA-pinned input);
rebuilding the unchanged provider with these glyphs exactly reproduces its
delivered SHA-256. Host FreeType regeneration produced different glyph metrics
and was rejected. The recovered arrays retain the existing OFL font license.
The host tests do not establish physical device timing or touch performance.

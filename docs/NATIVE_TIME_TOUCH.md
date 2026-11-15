# Native time on touch displays without an RTC

The explicit `native-time-touch` Settings profile uses Runtime realtime-control
for read, validated timezone/DST conversion, seed and bounded readback.
There is no RTC acquisition and no fabricated boot timestamp. Civil time is
unset on a cold boot until supplied. Timezone preferences use the established
key-value namespace; successful clock setting does not persist elapsed time.

Build with `scripts/build_portable_settings.py --settings-profile native-time-touch
--native-time-runtime-repo RUNTIME --display-rotation 0 --safe-viewport
--full-frames --return-app default.elf --output-dir OUT` using the pinned compiler.
The native SDK bytes are staged from the builder's immutable Runtime SDK pin.
The existing X4 profile and its RTC contract remain separate and unchanged.

`--safe-viewport` on Settings and Springboard selects capability safe insets.
Full physical RGB565 surfaces and touch coordinates are preserved. Only content
layout is translated. A contact beginning outside or crossing outside the safe
rectangle cannot become a new press before neutral. Unsupported content sizes
fail closed. This option currently qualifies non-rotated RGB565 products;
legacy rotated paper profiles retain their existing geometry.

Tests: `python scripts/test_native_only_settings.py --runtime RUNTIME` executes
production native seed/readback/error handling in normal and ASan/UBSan modes.
`ASAN_OPTIONS=detect_leaks=0 python scripts/test_safe_viewport.py` checks generated
geometry and hit mapping. These are host tests, not device qualification.

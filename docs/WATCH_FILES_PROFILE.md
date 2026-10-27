# Watch Files: explicit native safe-content profile

The optional `build_portable_file_browser.py --watch-native-ui` profile renders
shared glyphs/primitives directly into the capability's native RGB565 surface.
The capability-reported safe insets define content width/height and physical
origin; there is no board-name lookup, hardcoded panel size, or resized completed
bitmap. The existing width-based Watch layout keeps square glyphs and anchors
the footer to extra portrait height. Raw contact coordinates subtract the same
origin once, then use the exact inverse of the glyph-rectangle transform.
Out-of-safe-region contacts cancel, not clamp. Content must be at least 240
pixels in each dimension and have at least 240 logical rows; unsupported small
or wide/shallow geometries fail admission instead of clipping controls.

The separate `--time-profile watch-native-time --native-time-runtime-repo PATH`
selects readonly native UTC/shared-IANA time. The header uses shared saved 12/24-hour preference (12-hour default with AM/PM)
or `--:--`; it never invents or seeds time. Files schedules
closed native samples at one-second foreground checkpoints and repaints only
when the displayed minute/availability changes. Old paper/Watch profiles are
unchanged. The new profile uses `watch-native-time-file-browser.json`.

First Badge composition selects `--read-copy-only --rgb-only --navigation
--return-app default.elf --home-app default.elf`, default `storage.installed-files@1` instance0,
plus the existing explicit foreground-Light helper options. It has no SD,
update, writable-volume or remote-sharing grants. `--home-app default.elf`
additionally selects direct semantic-Home cleanup from nested screens. The
selected Badge navigation provider maps LONG to Home and SHORT to local Back:
SHORT from preview returns to details; LONG closes safely and returns Home.
Root Back follows `--return-app`. The flag does not remap SHORT into Home.
An installed-files volume remains limited to Runtime's admitted manifest/ELF
inventory; file previews do not expose arbitrary filesystem roots.

Foreground Light only runs when the real Files controller reports no owned
file/directory transaction, modal provider or retained custody. RAM drafts and
navigation state remain in the same invocation; this is not durable Deep
recovery. The builder records full sources/defines/include roots and exact
native+foreground SDK receipts for whole-cohort host recompilation/admission.

Validation: `scripts/test_watch_files_profile.py --runtime PATH --output DIR`
(and `--sanitize`) exercises the real controller/adapter/native-time reader at
five native geometries, asymmetric safe insets, exhaustive footer/editor inverse,
round-aperture pixels, filter/preview/root return, three draft-preserving modeled
Light cycles, live-file inhibition and terminal retention. Provider tables and
the Light helper are models here; whole-product tests must also exercise the
actual helper/Runtime graph. Legacy `test_portable_file_browser.py` retains its
normal/sanitized/rotation/Quick profile matrix. Physical behavior is unverified.

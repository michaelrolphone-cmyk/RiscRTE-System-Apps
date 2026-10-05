# Seventeenth-entry preparation (Springboard 1.4.9)

The shared portable adapter admits at most 17 explicit catalog entries, up from
16. The original Springboard already owns selection/paging for up to 128 entries;
no array, gesture, storage or application model is copied or expanded here.
An over-bound catalog fails refresh as a whole rather than truncating entries.
Tests exercise actual adapter acceptance/access/launch of entry 17 and rejection
of entry 18, including cleanup under ASan/UBSan.

The new genuine FontAwesome `solid:f274` calendar-check raster is prepared for a
future admitted Timecard profile. Its exact pixels and dimensions are extracted
from the already-vendored FAClassicSolid_18.cpfont with SHA256
`7b2f820afb525f62c280292e63b01ef302e8392c1fe6fc707719588c41c8204c`.
The matching codepoint CSV identifies it as calendar-check. `additional-icons.json`
keeps this prepared glyph in future font regeneration, without adding an app to
the installed-catalog registry. Existing glyph and text raster bytes remain
unchanged; the font license and upstream provenance are retained.

No Timecard catalog/launcher placeholder is added. The existing delivered
catalog fixture and registry are unchanged. The separately selected Timecard
Watch profile still requires the real app-data backend, explicit new partition
layout, 19 Runtime policies, complete app manifest/grants and integration/custody
checks. Current Watch dependency pins and delivered images are untouched.

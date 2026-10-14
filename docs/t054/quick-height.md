# Shared Quick native-height reconciliation

Port the Quick logical-height layout from 076f8da38f7dfebdfde6dd5d46e06446470fb828 onto the current T054 renderer, retaining its sliced raster API and paper icon implementation. Default logical height remains 240. Enable the deployed shared Home watch_face key in the explicit USER preference controller.

Validation: 271 byte-for-byte complete and sliced renderer cases against c77cd2d at 240 pixels, native 410x502 geometry/surface bounds, and all 49 existing RGB/X4 atomic Quick controller cases pass ASan/UBSan. The oracle baseline renames its row-render symbol to link both versions; asset bytes remain identical. These are host tests, not device qualification.

This checkpoint covers Quick only. Native-height Settings and launcher product reconciliation is separate and is not claimed complete here.

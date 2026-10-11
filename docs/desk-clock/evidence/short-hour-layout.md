# Short-hour raster baseline

The recovered clock renderer omits the leading hour zero in both 12/24-hour
formats. Its Segments face also resizes and centers the three-digit time.
The frozen Reader reference predates those changes, so comparing every digital
raster to that reference incorrectly fails at 00:00.

The fixture now composes the frozen Reader glyphs into the current layout for
short digital hours and requires exact pixel equality. Other digital/invalid
cases still compare exactly with the untouched Reader. Analog comparisons keep
their existing one-pixel / 512-pixel bounds. All 396 portable rasters retain
strict SHA-256 golden checks.

The baseline refresh changes 44 short-hour rasters only. Every frozen Reader hash,
all other portable hashes, and the saved PNG evidence remain unchanged. Production
clock code and the frozen Reader sources are unchanged. Normal, ASan/UBSan and
the no-import Xtensa renderer build pass.

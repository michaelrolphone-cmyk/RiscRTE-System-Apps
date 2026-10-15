# Optional telemetry scene suffix (scene-host 0.4.0)

Design reference `obd-dashboard.html`, SHA-256 `9ca9bf25f1b4bd6bc4c3ccf7eebf5a463747e94314d06b5cb4dea3f314a26770`.
This is a generic presenter extension, not an OBD service. No PID, ECU, vehicle,
transport, history or threshold logic belongs here.

`RiscSceneTelemetryV1.h` appends a size/tag/version checked suffix after the
existing reading-links API. Every prior API prefix and document struct is
unchanged. A component document with TELEMETRY flag can use INSTRUMENT,
BAR_GAUGE, METRIC_TILE, PLOT, TAB_BAR, VALUE_HERO, STAT_TRIPLE, LOG_LINE,
TRACE_LEGEND, STATUS_TILE and CODE_ROW. Other components remain reusable.

Present takes one complete document plus a bounded copied attachment (11 curves,
151 evenly spaced points per curve). Samples use normalized 0..10000 and 65535
for gaps. There may be one curve per tile or up to three solid/dashed/dotted
curves per plot. Node targets, styles, values, sizes and duplicate curves are
validated before updating either document or attachment. No app pointer is
retained. A regular component update clears the attachment. Model/frame copies
keep an in-flight raster independent of later app mutations.

INSTRUMENT: label=value, text=unit, choices=caption|value|caption|value.
BAR_GAUGE: label/text, value/maximum; current style shows eight subdivisions and
an upper hatched region. METRIC_TILE: label/value/badge(unit), optional curve and
danger/muted tone. PLOT: value=span(seconds1..600), choices=top|bottom scale labels,
step bitmask for high/low reference lines, maximum/minimum normalized levels.
TAB_BAR: four choices, value0..3, optional fault/count badge; emits VALUE events.
TRACE_LEGEND: label/value/unit badge/range choices and value=stroke style.
STATUS_TILE: value0 complete,1 incomplete,2 unsupported. CODE_ROW: code label,
wrapped description text and secondary classification choices. These terms are
visual semantics and can be used by non-vehicle applications.

E-ink layout uses the supplied 480x800 composition: large Orbitron value,
scaled RPM-style bar, two-column tiles, plot line patterns, clipped scrolling
content and an independently hit-tested bottom tab bar. Persistent footer and
Back targets are excluded from scrolled content hits. The existing Orbitron and
Rajdhani sources generate checked-in C raster data. The SVG icon paths retain
Font Awesome CC BY4 attribution. Original font files are not included in delivery
archives. Developer generation uses Pillow12.3.0/CairoSVG2.8.2; production builds
use the committed raster arrays directly and never need network fonts.

Run `scripts/test_scene_telemetry.py --runtime R` for ordinary and ASan/UBSan
checks of ABI discovery, atomic copies, bad sizes/nodes/values/styles, real tile
and tab touches, stale sessions/revisions, retained checkpoint no-repaint and
three-series graphs. `telemetry_probe.c` is a test-only adapter that locates real
visible hit boxes; tests still deliver DOWN/UP events through the native host.
Existing scene/Reader/component/lifecycle suites continue to cover their prior
contracts. Neither host tests nor raster captures establish hardware execution.

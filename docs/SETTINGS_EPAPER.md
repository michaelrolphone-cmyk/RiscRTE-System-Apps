# Nova7 E-Ink Settings 1.3.0

The shared Settings model/controllers now select a static, monochrome paper
presentation through the same display capability gate as Springboard. It uses
88px rows and controls, 3px rules, inverse selection, explicit Cancel/Save, and
reported device dimensions. Native 800×480 MONO1 output maps to logical 480×800
with `--display-rotation 90`; touch remains in the reported portrait space.

Time/date, 12/24-hour format, DST repeated-time decisions, About, and explicitly
compiled sleep/alarm preferences reuse their existing controllers and storage
contracts. No live RTC update or preference write occurs just from drawing,
selecting a draft, dragging, or cancelling. Physical navigation remains live.
Lists change pages only after completed gestures and do not animate. Displays
with fewer visible rows page rather than clipping actionable fields.

`python scripts/test_settings_paper.py` adds 15 ASan/UBSan scenarios using the
real adapter and controllers: render, format commit/cancel/failed write,
calendar edit with leap-day clamp and verified RTC write, draft discard,
inherited contacts, drag cancellation, snapshot-only page gestures, queue/poll faults, About, sleep choice
commit/cancel, bad native stride and complete teardown. Existing 272 Watch
Settings regression executions remain unchanged. Host evidence is not hardware
qualification or an installed bundle.

The paper font licenses/provenance and display-rotation definition are retained
in target build evidence. The production-generated frame is
[epaper-settings.png](nova/screens/epaper-settings.png).

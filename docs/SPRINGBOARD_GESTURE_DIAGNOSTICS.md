# Explicit Springboard terminal gesture diagnostics

`--springboard-gesture-diagnostics` is default-off and requires the existing
`--native-performance-trace --watch-native-ui` selection. It does not select an
older Runtime, change the app profile/version, alter axes or grant capabilities.
This is a separately named diagnostic schema, **not generic performance metrics
and not a movement stream**. It reuses the current 273 owner-only RAM trace
transport and explicit `perf` capture. No serial output, logging callback,
allocation, provider read or per-poll trace call is added.

The generic metrics API still excludes coordinates. Only this explicit
Springboard profile can summarize a gesture that the Nova application actually
owns. QuickActions, alarms, inherited held contacts, reset/GAP, navigation and
retained/failure boundaries discard the summary. There is no keyboard, password,
Wi-Fi form, preference value, path, string or pointer capture.

## What the diagnostic means

The adapter copies decoded provider coordinates and sequence/contact identity
with the same reduced input sample it queues. `np_poll` copies that sidecar with
the actual delivered contact. Nova supplies its actual signed logical dx/dy
immediately after `sb_motion_drag`, without recomputing or changing motion.
Coordinates are **decoded provider coordinates**, not raw CST report bytes.
Release has zero dispatched drag delta: subsequent attraction/snap is not
misrepresented as finger movement. The existing explicit 0/180-degree mapping
and actual integer logical scaling remain untouched.

Fixed storage retains at most four semantic positions: DOWN, first nonzero
movement, first sign reversal, and UP. It emits only once, at a valid terminal
release. Duplicate level polls do not append records. It does not create, end or
replace the recorder's existing interaction identity. Invalid geometry,
sequence/contact discontinuity and custody uncertainty suppress diagnostics,
without changing input handling. It adds no delay, event queue or frame cache.

## Schema 1 over existing COUNTER transport

High-byte tags `E0`–`E4` explicitly classify these words as gesture diagnostics.
They are not generic counter values. At most 14 records are emitted per gesture:

- `E0`: schema in bits 21–23, rotation 180 flag in bit 20, width-minus-one in
  bits 10–19, height-minus-one in bits 0–9.
- `E1`: selected semantic-slot mask in bits 8–11; contact ID in bits 0–7.
- `E2`, `E3`, `E4`: decoded provider, delivered mapped and dispatched delta pairs.
  Slot is bits 22–23; kind is bits 20–21 (DOWN 0, MOVE 1, UP 2); X is bits 10–19 and Y
  bits 0–9. Delta components are signed ten-bit two's-complement values, bounded
  by the unchanged actual +/-256 logical clamp.

Slots 0/1/2/3 mean DOWN/first-motion/first-reversal/release. Coordinates must fit
declared dimensions 1–1024. No coordinate is silently truncated to fit the wire.
`scripts/decode_springboard_gestures.py CAPTURE.log` decodes only this explicit
schema, verifies ordering/geometry/ranges and reports incomplete or overwritten
groups. Record timestamps are summary-emission times, **not physical contact
timestamps**. The existing fixed recorder and its lost count remain authoritative;
this is best-effort bounded evidence, not guaranteed retention.

## Owning checks

`test_springboard_gesture_trace.py` compiles the actual selected CST provider,
reducer, adapter and Nova against staged canonical Runtime headers. Modeled bus,
clock and display media drive all four held directions/reversals, both rotations,
release/snap and malformed-report recovery. Diagnostic-on/off paths must match
all completed framebuffer hashes and input/provider outcomes. Normal and
ASan/UBSan lanes are separate; no hardware speed/usability claim follows.

The pure helper tests bound 10,000 moves to 14 terminal records and exercise
invalid geometry, contact/sequence discontinuity, inherited input and delta
bounds. Decoder tests reject malformed/incomplete schemas. Local Quick owning
parity selects the diagnostic alongside actual Springboard presentation ownership
and asserts that no gesture-coordinate words escape modal/cancel/retained paths.
Target import/structural checks and integrated Runtime tests remain separate.

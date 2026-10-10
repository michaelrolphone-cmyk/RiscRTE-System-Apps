# Home 0.3.17 source reconstruction

Home 0.3.16 was delivered from unavailable source commit
024d661848bbbd5efc39e88d0e98118df249e5fe. The preserved receipt records 68
source inputs. The source audit recovered 59 exact blobs; 41 inputs already
matched the current System base. The unavailable inputs include the Home Points
and typography includes, sparse controller, desk codec/settings/raster and build
recipe. This change is a reconstruction and successor, not a claim that the
0.3.16 source or delivered binary has been recovered exactly.

The recovered paper_clock.c supplies the completed-tap feedback behavior.
Current shared adapters remain the integration base. The frozen unstripped ELF
(SHA-256 8ddfb1767d95566356f8b0fb5854e3b25c3a142fffdec467469e9590c88a79a6)
provides evidence for the Points landscape coordinate transforms and schema 2
wire layout. The original 128-byte schema 2 layout is preserved only as a separate
compatibility fixture. New source never treats it as a filesystem-catalog view.

The selected successor uses a copied alarm.service catalog projection pinned to
Utilities 40ed79bfb67f7bd75c68bf1ddb567469d51a51ff. It retains the previous event
and four upcoming events with stable 32-bit event/type IDs, item revisions,
32-byte labels and explicit exclusive valid_until. The four-row limit is a
presentation window; it imposes no event editor or filesystem catalog limit.
The independent PortablePointsState legacy ABI is unchanged.

Schema 3 is an explicit 408-byte little-endian record: an 80-byte Clock envelope
followed by 328 bytes of projection values. It contains no pointers. The optional
Runtime retained API must advertise capacity at least 408 bytes. Legacy/default
Runtime retains its 128-byte record and must refuse unsupported selected records
without consuming them. App sleep transport and foreground alarm-client access
are integrated separately from this rendering checkpoint.

The NOVA time uses the existing pinned Orbitron variable font at weight 900:
100 px for foreground Home and a newly generated native 80 px desk raster. Dynamic
labels use measured glyph advances and at most two lines; their stored values
are not shortened. Numeric face hours are unpadded; minutes remain padded. The
wake instruction is removed from face rendering. Selected preference default is
Clock+Points, face 6, while saved valid face preferences are respected.

Verification at the rendering checkpoint:

- Actual renderer, every 31-character repeated printable-ASCII label, all four
  upcoming rows, native glyph bounds and exact inverse landscape pixels.
- Schema 3 round-trip, all five full labels/IDs, malformed/canonical encoding,
  expiry, clock rewind, and distinct schema 2 compatibility fixture.
- Both checks run normally and with ASan/UBSan and produce identical pixels.
- Existing actual Clock/adapter suite passed six faces across 62 fresh-process
  minute cycles, including normal/sanitized boot, manual/timer, refusal and
  retained failure paths.

The rendering/codec proof does not qualify physical wake timing, backlight,
native RTC capacity, complete application custody or hardware. Those remain
separate integration checks. Frozen Home 0.3.16 artifacts are unchanged.

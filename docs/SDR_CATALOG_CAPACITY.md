# SDR catalog opt-in (Springboard 1.4.10)

The SDR Watch catalog has 18 visible entries. Compile only its Springboard with
`-DPORTABLE_CATALOG_LIMIT=18`. The shared adapter otherwise retains its historical
17-entry bound and the same generated machine code. The later HID-enabled Watch may explicitly select 20. Values outside 17..20 fail
compilation. A count above the selected bound fails refresh as a whole, reports
zero entries, and rejects get/launch without reading the catalog or calling the
runtime. This does not discover, install, or authorize any app.

The standalone Springboard builder exposes `--catalog-limit 18` and records
`catalog_limit: 18` in `springboard-build-record.json`. Its empty demonstration
catalog is unchanged; a deployment must supply its own admitted 18-entry catalog
and record the define in its build evidence. No other app should receive this
opt-in. The compile-time ceiling and Springboard's existing 128-entry UI capacity
are separate; this change does not alter UI arrays, focus, gestures, icons, or
paging. Springboard 1.5.0 focused-icon work is not part of this change.

`test/native_apps/portable_catalog_capacity_test.c` exercises the real adapter
with a fixed 18-entry backing array. The normal and ASan/UBSan matrix covers
implicit 17, explicit 17, and opt-in 18 with advertised counts 0, 1, 16, 17, 18,
19, 128, and UINT32_MAX. It checks every admitted manifest and launch, the
seventeenth Timecard and eighteenth Waterfall entries, unavailable app/runtime
launch refusal, null output, out-of-range indices, and lifecycle cleanup.
The runner also verifies unsupported compile-time limits are rejected.

The Watch 1.0.2 tag-publisher tests use the exact immutable manifest fixture from
PR #53 (`3e2c192961e46ebe83e04fb5fc99a098669a93a9`), rather than pretending a
current checkout is `git show` at the original release. Existing release manifests,
source SHA, component versions, and tags remain unchanged.

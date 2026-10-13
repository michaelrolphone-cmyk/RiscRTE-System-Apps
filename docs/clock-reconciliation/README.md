# Foreground Clock RTC reconciliation

The sparse X4 Clock previously displayed native realtime indefinitely after its first seed. `recover_rtc` intentionally reads RTC only when native time is UNSET; a VALID native clock could therefore stay minutes ahead of the accurate RTC shown by Settings.

This change is based on System `8ecf587d81763cd6b4d7d4b0d4324c5e8b4d68f2`, retaining production `a694daefe499d58176bd187c3f6da1bef5d7051f` and the stronger PR110 alarm-loading checkpoint at both loading sites. The delivered .105 inline patch covered only the first site. The shared recovery API keeps its prior contract; a new explicit reconciliation API is selected by sparse Clock.

## Behavior

- One foreground RTC attempt per 60 seconds of native monotonic time, with no wait loop or yield. Repeated reads never add artificial wall-time increments.
- A new foreground invocation, settings hydration, or confirmed sleep return makes the next settled read eligible. Active/queued raster frames defer RTC work; ordinary native reads continue.
- RTC and native readings must fit a 250ms acquisition/read/release bracket. Differences within two whole seconds preserve the native fractional phase. Larger differences in either direction cause one checked native seed from the interpreted RTC, after releasing RTC custody. Seed readback must be within 250ms of the supplied whole second.
- Timezone, RTC UTC/local basis, range, ambiguous fold and nonexistent local time rules are unchanged. Native time is never written to the external RTC.
- Safely rejected malformed calendar or slow samples leave a valid native display available and retry at the next minute. A boolean RTC read/acquire failure cannot prove clean custody; the existing sticky retention fence remains. This API cannot safely treat those failures as transient and retry them in-place.
- Unpromoted timer-only deep wakes do not acquire RTC or load preferences. Reconciliation happens on promoted foreground/GPIO wake. Continuous unattended timer-only operation retains the original native-time authority; this change does not recalibrate the hardware oscillator or claim to correct that domain during every timer wake.
- Sparse Clock package version is 0.4.14; non-sparse builds retain their versions.

## Verification

`test_portable_realtime.py --reconcile` includes all original 123 client scenarios plus +/- five-minute drift, 1200 frequent refreshes, 1440 minute samples, forward/backward settings changes, modeled light/deep foreground wake, timer denial, invalid/slow/uncertain RTC, and seed readback failure. Normal and UBSan runs pass on macOS.

`test_sparse_rtc_reconcile.py` links the actual production Clock, adapter, and X4 sleep hook. It passes 20 lifecycle/retained cases and 54 cold/GPIO/deep-entry six-face comparisons normally and under UBSan, comparing rendered pixels from accurate native time versus +/-300-second drift. The same current fixture against original 8ecf587 fails at the first cold +300s case because no RTC read or seed occurs. Source-hashed client receipt and logs are under evidence.

Run from System:

```
python3 scripts/test_portable_realtime.py --runtime-repo ../RiscRTE --sanitizers undefined --reconcile
python3 scripts/test_sparse_rtc_reconcile.py --runtime ../RiscRTE --x4 ../RiskRTE-XTEINK-X4-PRO --sdk ../reader-clock-sdk/sdk/driver --sanitizer undefined
```

Host dependencies: Runtime `a91edf3fada6a559ec107186dd163d0c8da10d29`; X4 sleep hook from `8dba3205f63dcb9558d80890b9bf5c4aed16f230`; five lifecycle SDK headers from Reader `c98e4222048244b1e729869ed47aa99130025fd3`. The hook SHA-256 is `0858a85e56bb12ef1255b02eeb5fb3618a4e46fd283e0a369f4f3bffefe0462b`, matching the delivered .105/.106 Home receipt. The host fixture preserves its timer SD-close behavior. This source pin is not a replacement product image. ASan executables stalled on this Mac; Ubuntu CI runs normal and ASan/UBSan. Target ELF and physical display/RTC wake testing remain separate qualification gates.

## Product provenance

Recovered .106 assembly explicitly preserves all 43 internal ELFs and adds `obd_test.json`, resident foreground `obd_test.elf`, and `Apps/obd-test/obd_test.elf`. It is based on .105 BIN `77e203e07126b0526bc493be5cdab72fbb88279e9e20417b5e1785fcac89d1b8`; reported .106 BIN is `524a5fc3aa4bba634a373a82bed384969104d26778d192e8e8feca7b9b1407d2`. Both are evidence-record hashes, not newly reproduced images here. The recovered Utilities patch applies to `5c6698dcd1d874e7c57f0b0e6b1ee0dec0803165` and matches its four evidence files exactly; the helper's older a9c6dfea source field is stale.

The prior d3eb532 candidate is unrecoverable per parent recovery search. This is a new source repair, not a claim to have recovered that commit. No cumulative build, merge, release or device flash is performed. Build delivery remains held until 23:30 UTC on 2026-10-13.

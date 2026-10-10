# X4 cold-start logo

The explicitly selected sparse Clock presents the existing RiscRTE logo once on
cold/reset startup, after foreground promotion, typed display initialization and
orientation preferences. It presents one full frame and immediately continues
RTC recovery and Clock rendering. There is no animation or timed hold.

The nine-block mark comes from the frozen Reader `docs/logo.svg`; its full-size
geometry and the unchanged DejaVu Sans Bold wordmark rows come from Reader
`src/components/StartupScreen.cpp`. Exact source commit and SHA-256 identities,
the original SVG, MIT license and DejaVu notice are under
`lib/PortableApps/boot_logo/`. The build includes these notices and hashes.

The coordinated X4 `portable_desk_clock_boot_is_cold` private function reports
only the cause already read through the existing retained-wake capability. An
invalid, missing or unreported cause cannot authorize a splash. No new Runtime
export, capability, native hardware operation or retained record is introduced.

A splash also requires `RISC_PROVIDER_PROMOTION_OK`. The selected demand profile
promotes only once per Runtime boot; later default-app reloads return
`RISC_PROVIDER_PROMOTION_ALREADY_READY`. This suppresses the splash when returning
Home to Clock. Timer, GPIO and other deep wakes skip the splash even if they need
foreground promotion. Existing timer reconstruction and refresh accounting stay
unchanged. An uncertain splash acquire/submit/wait fences the invocation before
RTC recovery or further I/O.

`test_sparse_clock_startup.py` includes independent complete physical-frame checks
against the SVG geometry and pinned wordmark values, cold/reset first entry,
cold/reset reload suppression, timer/GPIO/other wake suppression and display
failure retention. It also runs the existing Clock/adapter/X4 composition suite
normally and with ASan/UBSan, target ELF validation, and unchanged ordinary
(non-sparse) ELF comparisons. `--x4-ref` selects the exact coordinated X4 hook
commit; the default pins the first compatible hook. Runtime's separate
`run_demand_activation_test.sh` covers promotion across a child-app handoff.
These are host and target-build checks, not physical X4 display qualification.

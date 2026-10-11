# Explicit local clock sleep hook

`build_paper_clock.py --local-sleep-source PATH --sleep-capability NAME
--sleep-sdk DIRECTORY` opts only the default paper clock into a deployment-owned
power hook. It requires `--navigation --alarm-client`, emits a separate0.2.1
Clock manifest and an explicit named power requirement, and records the hook
source hash. The deployment must provide the exact corresponding boot grant.
Without these options the existing0.2.0 controls clock keeps its unavailable
sleep action and compiles byte-identically.

The generic adapter handles a completed crown action after Quick Actions has
had a chance to consume it, but before any Home handoff. It uses the existing
portable sleep lifecycle: drain/release foreground resources, call the local
hook, preserve native retention, then reopen touch and reset navigation on
ordinary refusal or wake. This selection is manual-only; it does not add idle
sleep to the clock or sleep authority to navigation.

The X4 implementation and 28 real-clock integration cases live in the X4
platform repository under `minimal/apps` and `minimal/test`. The shared adapter
contains no Watch PMU casts or X4 power layouts.

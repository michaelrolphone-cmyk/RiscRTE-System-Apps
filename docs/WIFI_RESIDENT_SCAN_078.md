# Wi-Fi 1.1.25: keep active radio work out of resident POLL

X4 combined 0.1.78 receipt `8e9e8eb7a2b1f7f860777d0d0e8a89dd2822af87`
installs internal Wi-Fi 1.1.24. The app is not one of the SD applications.
Its SHA-256 is `35d9117e6eb6f54576f766eecb289d9f5d5b73776d99db1c6bf574059045cae5`.
Rebuilding System `5b28172e979b5f008da19e9a00a3b98e35937974` with
resident SDK `274bc66f193cbe29018d2a85c9400cb0dce8aacc`, pinned tagged
alarm headers and `--raster-snapshot` reproduces every allocated section,
including relocations, exported symbols, code, constants, data and BSS layout.
Only nonloaded container/symbol metadata differ. See the section-match receipt.
This repair is based on that source, not the newer Reader scene checkout.

## Reproduced failure

Available networks -> `wifi_activate(3)` -> `wifi_scan_start` -> copied async
`begin` -> Wi-Fi provider 0.2.1 -> CpuPort -> native mailbox/worker. The installed
app has the async suffix, saved profiles and shared masked keyboard. Native
Runtime is `7071f0c25e223f24670bed409a5f8b6f291c8042` (0.2.5), not the stale
0.57 `sources.lock.json` entry. Provider source is Watch
`a304597bf26491483bb0a567569f0ca0267cdf9c`; installed ELF SHA-256 is
`b966f6020cfd6bac0f990f8232c9b55c37f3dae6a488a608ff58b5f598e3da3e`.

The native worker owns SDK setup/scan/cleanup. Callbacks publish atomics;
owner polls consume copied snapshots using a nonwaiting mailbox lock. Its
permanent task yields between iterations. Cancellation stays pending until
cleanup finishes; it does not delete the task or unmap live code. Existing
bounded stage records do not log credentials or call blocking logs on the worker.

After presentation settles, adapter input polling calls the resident POLL
checkpoint. `portable_wifi_services_safe()` allows healthy async work, including
STARTING/SCANNING/RESULTS/STOPPING. It therefore lets that checkpoint through.
Runtime `residentCheckpoint` calls `appExitBarrier` before switching invocations;
CpuPort `appExitSafe()` is false while `c.active` is set. The barrier revokes
invocation authority and retains the app/providers. Policy-inhibition flags
cannot prevent this: the barrier runs before host policy dispatch. The result is
a frozen foreground even though the native worker can remain healthy.

The regression fails on the exact baseline when active Wi-Fi reaches the
synthetic resident dispatch boundary. Inspection of the selected production
Runtime establishes the destructive retention at that boundary. This is a
reproduced source defect, not hardware confirmation of the reported incident.

## Focused change and verification

After reconciling Wi-Fi progress, defer only resident POLL while async ownership
remains. Keep input/render/cancel polling local and mark activity for the next
safe host checkpoint. Results and connected sessions still own radio resources.
Resume checkpoints after confirmed quiescence. Explicit modal cleanup and
Runtime's barrier are unchanged. Other app compilation paths are unchanged.

Run:

```
python3 scripts/test_wifi_resident_checkpoint.py --runtime /path/to/runtime \
  --watch /path/to/watch --output /path/to/evidence
python3 scripts/test_wifi_workflow.py --output /path/to/workflows
```

The new runner executes normal and UBSan cases: delayed scan, results, timeout,
failed setup, cancellation/reentry, connection, retained cleanup, continuing input
and render scheduling. Its companion links the actual app/checkpoint, provider,
CpuPort and native worker while `wifi_init` is blocked. Resident dispatch and
SDK/display/input/storage boundaries are synthetic; this is not an RF emulator.
The existing native suite also covers callback gating, worker startup failure,
mailbox contention, cancellation, reentry and SDK cleanup failures. No new
runtime/provider code or logs are needed. No hardware, flash or merge is claimed.

Build only Wi-Fi 1.1.25 from this branch. Preserve `--raster-snapshot` and the
selected resident/policy/navigation/scrolling/native-time/telemetry options.
Use the historical resident SDK above, compiler GCC 8.4.0 esp-2021r2-patch5,
Utilities `e80172353fcb9e6519ca9d9b2a4d43c69c1bc91d` headers. The X4 companion
source checkpoint records the exact command, output hashes and replacement
instructions. Ordinary nonresident Wi-Fi versions are unchanged.

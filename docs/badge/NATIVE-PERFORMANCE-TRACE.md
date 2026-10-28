# Explicit native bounded RAM performance trace

`--native-performance-trace` is default-off on the Settings and Springboard
builders. It accepts only `native-time-touch` Settings or `watch-native-time`
Springboard. It does not switch to the historical diagnostic profile, change
application version, enable plain stage logging, or replace the selected SDK.
The final staged Runtime header must exactly match Runtime
`273bdbccf65647a803ab0586a418f8283732baec`; its matching Performance header is
staged alongside it. The current foreground-idle SDK selection supplies that
prefix. Mismatches fail rather than silently changing the ABI.

The kernel must independently select `RISC_PERFORMANCE_TRACE=1`. The build
receipt uses `native_performance_trace`, deliberately distinct from the older
`performance_trace` SDK/profile path. Capture is requested using ASCII `perf`
followed by LF (hex `70 65 72 66 0a`), through the existing Runtime serial
protocol. This selection does not issue that request or transmit spontaneous
performance logs. Runtime owns the fixed-capacity ring, overwrite/lost counts,
phase aggregates, partial-write replay, and owner-thread restriction.

## Granularity and limits

Existing PortablePerformance hooks count input reads, clock/KV/battery reads,
raster passes and display status polls locally. They do not call Runtime,
providers, serial, health or yield per pixel to count pixels. Trace callbacks
occur at interaction/action, draw begin/end, submit/observed completion and
bounded counter snapshots (at most six counter records per snapshot). Native
Runtime and panel timing remain separately selected kernel instrumentation.

Input timestamps mean accepted software samples, not physical contact time.
Completion means observed display-token completion, not photon timing. This
selection does not add CST per-read elapsed timing, Home app draw hooks or a
frame-durable crash log. The ring is ordinary RAM; Deep reset loses it. Events
can overwrite the ring; aggregate/lost information must accompany analysis.
Trace callback cost and ordinary local counter increments are not free.

## Owning evidence

- `tests/test_native_performance_build.py`: default-off no-I/O behavior,
  profile/mixing rejection, exact-prefix preservation and builder ordering.
- `scripts/test_native_performance_selection.py`: real selected 466px Settings
  geometry with 328px safe content, all fixture scenes and actual controller
  actions, trace off/null/enabled, normal and ASan/UBSan. Exact framebuffer
  hashes and behavior match; enabled fixture emits 378 records total.
- `scripts/test_native_springboard_trace.py`: actual Watch-native launcher,
  native clock, interrupted/repeated input and missing/invalid clock cases.
  Fourteen scenarios times three trace modes times two sanitizer lanes (84)
  preserve every captured frame hash and action/launch summary. Enabled cases
  emit 1–246 records, not per-pixel callbacks. This owning fixture uses the
  synchronous frame path; the target build also selects sliced native frames,
  whose installed-product qualification remains an integration gate.
- Both current-profile target builds with the flag passed target layout,
  structural ELF and import/export validation. Neither is a flashed result.

One host run of the 14 normal launcher processes totaled 127.5ms off, 152.7ms
null and 141.6ms enabled. Process startup/scheduling noise and instrumented
frame hashing dominate these small samples: this is not a reliable device
or isolated instrumentation overhead estimate. Runtime's own emitter benchmark
is separate; full target timing must be captured from the running device.

The older paper trace script fails on this merged cohort at its pre-existing
launch pending-token assertion; its isolated Settings section also exceeds the
old timezone fixture poll ceiling. Those attempts remain failures, not passes.
This change does not modify their product/controller sources or mask either
assertion. The selected native tests above establish their stated scope only.

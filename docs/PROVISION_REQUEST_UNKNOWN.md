# Native provisioning ambiguity

The Installed Apps client honors the native request API's return code before
interpreting any status fields. An ambiguous queue result is not retried in the
current invocation. Subsequent refreshes receiving `UNKNOWN` must not infer
`PENDING` or `CONSUMED`, ask for credentials, or resubmit the request. A native
implementation may require the next ordinary boot to reinitialize storage
selection. The service never resets the device or repairs private native NVS.

`test/native_apps/installed_apps_boot_profile_test.cpp` covers a queue returning
`UNKNOWN`, followed by nine refreshes with repeated status `UNKNOWN`. The status
fixture deliberately supplies NONE, PENDING and CONSUMED fields to prove that
stale output cannot override the failed status result. Every refresh leaves
queue-call count unchanged, keeps profile credentials cleared, and closes its
HTTP session. Only an explicit fresh-native-boot fixture reset permits the
subsequent independent scenarios.

This service regression runs alongside the existing 100 SD interruption/retry
cases, absent/exported media fallback, changed profiles, exact native binding,
and retained-custody fences. It does not qualify the native persistence protocol
or physical hardware.

```sh
RUNTIME_REPO=/path/to/Runtime bash scripts/test_installed_apps_boot_profile.sh
ASAN_OPTIONS=detect_leaks=0 RUNTIME_REPO=/path/to/Runtime \
  SANITIZE=1 SANITIZERS=address,undefined \
  bash scripts/test_installed_apps_boot_profile.sh
```

Both commands passed for this checkpoint. Leak detection was disabled for the
container run; address and undefined-behavior checks remained enabled.

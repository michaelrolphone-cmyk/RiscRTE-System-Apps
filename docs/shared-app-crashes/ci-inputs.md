# Reproducible shared custody host qualification

The workflow `.github/workflows/shared-data-custody.yml` stages the exact five
SDK header bytes used by the independent regression. Its source pins are:

- RiscRTE `4107eaeedd8c801eeaf7deb1c7bc4a943cd27342`:
  `sdk/app/RiscRuntimeV1.h`, `sdk/app/RiscRealtimeV1.h`, and
  `sdk/app/RiscAppDataV1.h`.
- RiscRTE-Utilities `cc56de34f2a1c6efa9761b28d92ed1cb8e8240e5`:
  `lib/Alarm/include/AlarmServiceV1.h` and `AlarmServiceV2.h`.
  The same pin supplies the `Apps` and `lib/Alarm/include` include roots.
- Unchanged System baseline `ffe0295eff869f0d33d56f98548f9b435975d584`.

Header SHA-256 values are checked in the workflow before compilation. The
runner receipts also record compiler, input hashes, exact compilation commands,
and each result. No SDK headers or production source are rewritten by tests.
The existing Contexts fixture's main function is renamed in a temporary test
include; its included adapter remains the selected repository's actual source.

After reproducing the workflow's SDK staging, the fixed-source command is:

```sh
python3 scripts/test_shared_data_custody.py \
  --sdk-include build/shared-custody-sdk \
  --app-data-sdk build/shared-custody-sdk \
  --utilities-root .deps/shared-custody-utilities \
  --output build/shared-custody-ci/fixed --callback-audit
```

The unchanged-baseline command adds `--source-root .deps/shared-custody-baseline`
and `--test-source test/native_apps/shared_data_custody_test.c`, replaces
`--callback-audit` with `--baseline`, and uses a separate output directory.
The baseline assertion expects the first denied optional namespace to retain
custody. This establishes the original failure rather than counting it as a
fixed-source pass.

The fixed suite runs 24 cases in normal and ASan/UBSan modes on four host
profiles: 800x480 MONO1 asynchronous and synchronous paper, 240x240 RGB565, and
410x502 RGB565. RGB profiles select native-height, color-only, Nova UI and
asynchronous RGB adapter flags. The original failure reproduces on all four.
Local results: 192/192 fixed assertions pass, 8/8 baseline assertions reproduce
retention. The suite covers namespaces 2/3/4, dirty denied grants, wrong versions
and namespaces, lost Runtime identity/health, refused storage release, and
retained-success callbacks at all four CONFIG paths plus capture raster/input/
presentation and the enclosing Contexts step caller.

This proves shared-wrapper behavior with strict host providers. It does not
establish that a product's deployed ELF contains this source. Product acceptance
still needs its release/app SHA, pinned source and SDK closure, actual compiler
flags, target rebuild, and hardware launch/render/settings/return tests. Native
provider behavior and touch-coordinate mapping are outside these host tests.

## Existing generic-client failure

On both unchanged `ffe0295` and this repair, `context_client_test.c:93` aborts:

```text
Assertion `portable_contexts_step(&c,true,true)&&!last_policy.radio_allowed' failed.
```

Reproduction, run with ROOT set separately to the baseline and repaired trees:

```sh
cc -std=c11 -O1 -g -Wall -Wextra -Werror \
  -I"$ROOT/lib/PortableApps/include" \
  "$ROOT/test/native_apps/context_client_test.c" -o /tmp/context-client
/tmp/context-client
```

The existing `scripts/test_context_client.py` uses the same normal compile and
run before its sanitizer phase, and stops on this assertion. No assertion or
production behavior was changed to hide this existing failure. It remains
separate from the 24-case shared-wrapper regression above.

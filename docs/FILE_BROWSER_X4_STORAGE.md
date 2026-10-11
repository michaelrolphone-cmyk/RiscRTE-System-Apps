# Files 1.5.8: selected X4 SD authority

The previous selected paper-motion build compiled
`storage.installed-files@1`, instance 9, while X4 granted installed-files instance
0. Runtime correctly refused acquisition. SD is the separate `storage.volume@1`
provider at instance 9. The old motion host test independently hardcoded volume9,
so it did not execute the target's compiled selection.

Build the selected Files artifact from a clean source commit:

```sh
NATIVE_APP_CC=/path/to/xtensa-esp32s3-elf-gcc \
python scripts/build_portable_file_browser.py \
  --time-profile x4-native-time \
  --native-time-runtime-repo /path/to/Runtime \
  --tagged-alarm-utilities /path/to/Utilities \
  --alarm-client --quick-actions --paper-transitions --stage-logs \
  --home-app default.elf --return-app springboard.elf \
  --storage-capability storage.volume --storage-instance 9 --file-handlers \
  --output-dir build/x4-files-158
```

The manifest declares volume API1 and file.open API1. The build record and native
receipt both record `storage_selection.primary` as volume/API1/instance9 and
`secondary: null`. The product grants must be volume9 and file.open0. There is no
USB selection, registry-order fallback, or implicit installed-files grant.
The builder rejects nonzero installed-files instances before compilation.

The X4 product's `file_browser_admission.validate` checks the original ELF hash,
source identity, manifest requirements, exact compiled storage defines, build
record grants, native receipt selection and generated boot grants before ELF
compaction. Update the product's app source lock, catalog and expected Files
version to this clean artifact as part of cohort integration. Do not reuse the
1.5.7 ELF or its receipts.

## Shared cleanup fix

Destination Cancel now retries a retained copy close before changing volumes.
Failed close preserves the original handle, grant, and commit/abort decision;
repeated operations cannot overwrite that ownership. Retrying a successful
copy's failed commit close finishes that commit, while an incomplete copy remains
an abort. This change is isolated in `Apps/file_browser_operations.inc` and the
raw-touch regression fixture so other portable deployments can adopt it without
the X4 storage composition change.

All newly built portable Files profiles carry version 1.5.8 for the shared source
change. Their existing capability selections remain unchanged unless explicitly
selected by the build command above. Legacy Reader sources/manifests are
unchanged. Changed portable ELFs are not claimed byte-identical to prior versions.

## Verification

```sh
ASAN_OPTIONS=detect_leaks=0 python scripts/test_file_browser_operations.py
ASAN_OPTIONS=detect_leaks=0 python scripts/test_portable_file_browser.py
ASAN_OPTIONS=detect_leaks=0 python scripts/test_file_browser_paper.py
ASAN_OPTIONS=detect_leaks=0 python scripts/test_file_browser_runtime.py \
  --build build/x4-files-158 --runtime /path/to/Runtime
```

The Runtime fixture pins c546dae32e2e75f7e7f4867dc788b6be53ded64c, loads the actual
Files controller in a host ELF, and takes its acquisition defines from the
verified target record. It projects the selected storage/file.open policy into
real Runtime/ProviderGraphV2 admission and module lifecycle. Fifteen scenarios
cover correct acquisition, deliberately wrong compiled selectors and policies,
missing provider, SD absence/errors, retained handle recovery, truthful no-handler
status, and a declared fixture receiver's `/sd/read.txt` handoff and fresh caller
return. Nested controller-loop cases navigate `/` to `/Books`, verify exact
preview bytes through `/Books/read.txt`, and send only `/sd/Books/read.txt` to the
broker/receiver. Volume callbacks reject an accidental `/sd` prefix. Media and
rendering are simulated. The receiver is not a deployed app.

Eighteen additional raw-touch `app_main` flows cover SD reinsertion/removal,
preview close failures, rename/move errors, copy commit/abort ownership, and
recursive delete failures. The operations fixture retains its existing direct
tests for previews, file operations and handler selection. The motion fixture
now compiles Files with the target record's actual defines.

ASan/UBSan remain enabled. LeakSanitizer is disabled in the current executor
because ptrace prevents it from running; fixtures assert handle/grant/frame/
subscription cleanup directly. No hardware, physical media, physical waveform,
external publication or complete product BIN is qualified by these checks.

The selected X4 bundle has no `supported_file_types` receiver. Open/Open with
therefore report no declared handler. Text/hex previews and folder/file operations
remain available without adding a document viewer.

# Files 1.5.32: protect released kinetic scrolling from resident policy

The shared Files controller now reports its contact/kinetic state to the
resident adapter. Background policy and idle checkpoints remain inhibited
until that bounded motion settles or an explicit action interrupts it.
Explicit Home, Back, controls, shutdown and cleanup remain available. Neither
the scroll arithmetic nor GT911 handling changed.

This is a reproduced **policy interruption of the kinetic tail**, not proof
that the reported local list snap-to-top has this sole cause. GT911 0.1.10
preserves the last contact coordinates in UP; neutral packets do not substitute
zero coordinates. A local Files close preserves the logical offset, while a
mounted peer's checked close may intentionally restore its local folder.

## Evidence

- `test_resident_system_clients.py --app files`: 18 normal / ASan+UBSan cases
  against production Runtime `1c1ed8661ea30d47e7e9707c41d492ce3a2b4d70`.
  New `scroll-policy` fails before the fix because POLL schedules policy while
  velocity is nonzero. It passes after the fix: coasting stays monotonic,
  POLICY remains busy during the tail, then ordinary policy is admitted.
- `test_files_scroll_gt911.py`: 16 normal / ASan+UBSan checks through the actual
  0.1.10 driver's register fixture, ordered consumer and Files scroll primitive.
  Fling, held release, reversal and bounds run at rotations 0 and 180. UP keeps
  the last MOVE location, momentum decays, and fresh taps still activate.
- `test_logical_scroll_latency.py --controller files`: 48 normal / ASan+UBSan
  checks of real Files/controller/adapter at 0, 17 and 2300 ms simulated present
  latency, two viewport sizes, touch/Confirm/drag/changed-item cases.
- `test_file_browser_mount.sh` with UBSan: nine RGB/paper/paper-scroll checks
  retain mounted browsing, preview, rename, mkdir, copy/move/delete, cancellation,
  retry/expiry, checked disconnect and the Watch eight-path export workflow.
- `test_portable_file_browser.py`: eight normal / ASan+UBSan checks retain
  pagination, sorting, filtering/keyboard, previews and storage error cleanup.
  LeakSanitizer is disabled because this executor is traced; ASan and UBSan run.

The old snapshot-only `test_touch_scroll_files.py --case drag` fails on the
unchanged baseline because its provider never publishes ordered raw edges. It
is not a release regression witness. The optional raster-snapshot variant of
`test_logical_scroll_latency.py` has a pre-existing 2300 ms frame-count assertion
failure (`presents == 1`); its normal ordered path is the 48-case result above.
Neither result is counted as a pass or silently weakened.

A development Xtensa GCC8.4.0 target build from `df01180b` passed real ELF
structural and import/export validation. All deployed1.5.31 defines, imports,
exports, 16 grants, storage selection and sharing profile remain identical.
This scroll-only target is not the final deliverable: the separate terminal
Files handoff correction must be integrated and rebuilt. No physical X4 test,
publication or flash is claimed. Product integration owns final qualification.
The delivered sharing/peer profile version advances from 1.5.31 to 1.5.32;
capabilities, all mounted features and existing storage selection are retained.

## Target rebuild profile

From this checkout, use the existing compiler and pinned SDK-source checkouts:

```sh
NATIVE_APP_CC="$XTENSA_CC" python scripts/build_portable_file_browser.py \
  --time-profile x4-native-time --native-time-runtime-repo "$RUNTIME" \
  --tagged-alarm-utilities "$UTILITIES" --alarm-client --navigation \
  --touch-scrolling --touch-rotation 0 --display-rotation 90 \
  --storage-capability storage.volume --storage-instance 9 --file-handlers \
  --return-app springboard.elf --home-app default.elf --ble-broadcast \
  --resident-shell-client --resident-runtime-sdk "$RUNTIME/sdk/app" \
  --resident-policy --raster-snapshot --stage-logs \
  --peer-files --webdav-sharing --sharing-export-instance 0 \
  --sharing-tcp-instance 0 --sharing-entropy-instance 0 \
  --sharing-setup-instance 0 --output-dir "$OUTPUT"
```

The builder reads the pinned native-time/Alarm source commits itself. Retain the
current product's resident SDK for `$RUNTIME/sdk/app`, all 16 grant bindings,
MONO1 portrait transform, staged sharing objects and real ELF validation.

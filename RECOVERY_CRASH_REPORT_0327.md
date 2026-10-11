# Home 0.3.27 crash-report recovery working copy

COMPLETE RECOVERED SOURCE TREE: this checkpoint contains the restored full .53 System baseline plus the recovered crash-report production delta, fixtures and provenance at their intended paths. Earlier partial checkpoints remain in history. No existing product binary is an input to the final build. The final clean build must begin only after all selected product-source recovery and integration are complete.

Fresh full product qualification is pending. Exploratory builds and host checks recorded during recovery are comparison evidence only; the user has required a new clean build after source recovery is complete. This is production source recovered after an executor reset, not a deployable product image or a continuation of the erased target qualification.

## Provenance

The baseline is the saved System Home 0.3.26 source from X4 0.1.53, originally local commit `24b07f92eb8bab1943c1e7ef146960e5d896b37b` and tree `c97d04836a5abbada97ca5161e26e138bba83f82`. The unavailable former crash-report commit was `68e014e15e806174ffbb396de7d0ea40ec25d925`. Source was recovered from available authored work history and restored project files. Recorded source SHA256 matches are itemized in `receipts/crash-report-recovery.json`. Git identity and prior target/test results are not claimed for the recovered commit.

## Selected feature

`--crash-report-sd --crash-report-spool-namespace 62` selects Home 0.3.27. Ordinary builds retain their earlier version and behavior. The additional `storage.app-data@1` requirement binds only dedicated namespace 62, owner `paper_clock`. Current capabilities and live-grant limits are preserved. Native admission requires the separately recovered Runtime capacity prerequisite selected with requirement rows 17 and policy rows 18.

A healthy Home copies the same logical crash-report body used by its OSD, including captured sequence, history and stack details when present. No panic-context filesystem work is added. The bounded internal AppData spool (`crash-spool.bin`) holds eight reports. Internal atomic replacement and exact readback must succeed before native evidence acknowledgement, and acknowledgement also requires completed OSD presentation and Continue. Continue still works if storage is missing or unavailable. Retry work is admitted at settled Home checkpoints, at most once per 30 seconds.

The shareable SD path is:

`/CrashReports/<64-hex-firmware-sha>-<boot-8-hex>-<sequence-8-hex>-<text-crc-8-hex>.txt`

The exporter uses exclusive temporary creation, checked sync and close, exact temporary readback, no-overwrite rename, and exact final readback before internal spool removal. It uses bounded chunks and callback admissions; an existing synchronous storage callback cannot be preempted by that budget. No whole-card scan or diagnostic logging loop is added.

SD must expose the append-only checked state observer supplied by the recovered SD 0.2.14 working copy. Missing/malformed observer tables defer without legacy SD I/O. The observer is sampled before and after callbacks so that an initial ready failure or final close cannot hide retained mutex/log/writer custody. Retained custody stops further I/O rather than pretending persistence succeeded.

A full internal queue preserves the oldest seven reports and admits the newest, recording a saturating drop counter and last dropped identity. OSD and exported reports expose that loss. If the internal spool is corrupt, unavailable or cannot commit, original native evidence remains pending. Its finite native buffer can then prevent a later crash from replacing that evidence; this limit is not concealed.

## Deployment boundary

This source checkpoint does not initialize, format, copy or erase existing AppData, NVS, alarm or SD records. A real preserving installation must first install a compatible native Runtime with its matching cohort receipt, retaining all existing user-data partitions and authority, then install the augmented Home manifest plus exact namespace-62 owner policy and matching SD provider. Do not use an empty initial-provisioning AppData image for this update. Existing RTC crash evidence tied to a changed firmware identity can be discarded by the native mechanism during a firmware change. Actual updater transaction, power-cut behavior and hardware testing are still unqualified.

## Fresh checks so far

Recovered spool, formatter and SD-exporter focused tests, the 15-case archive lifecycle suite, plus all 27 existing production OSD/Runtime/Graph cases, pass both normal and ASan/UBSan runs against the complete local recovered source. These are new host runs on the recovered source, not inherited results. Full target build, exact native admission and real-provider fault qualification still require fresh execution. The lifecycle fixture starts from the recovered exact 14-case test, then adds a freshly reconstructed checked-state regression; exact parity with the erased final 15-case fixture is not claimed. Prior qualification logs are historical only.

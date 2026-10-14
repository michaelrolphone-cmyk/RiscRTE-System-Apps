# T023 audit and implementation state

Starting cumulative baselines: X4 .121, Waveshare .38, T-Watch1.1.33, published/verified in t064-build. Preserve all product features and source-bound manifests.

Generic System-t054 includes cached installed metadata, copied asynchronous /Apps scanning, durable activation and provider journals, HTTPS feed, SD-first capacity selection and internal fallback. Those generic services are not active in all current products. X4 native lacks the later combined internal package integration; Wave contains default-off implementation. Watch must retain its NVS profile and no fabricated SD capability.

Gaps confirmed: no automatic first-start/profile reconciler; fixed X4 feed identity/URL in builder; equal app versions cannot request dependency-only updates; internal Apps folder is not scanned for externally added packages; foreground client requires an SD grant even where internal installation exists. Core bootstrap driver replacement must not bypass native graph or live-consumer custody. Product activation needs complete native/resource/admission verification, not merely a new service ELF.

Working source: System-t023, branch fix/t023-automatic-provisioning, clean clone of System99c62a0 (T064). Dependency-only provider updates implemented; no T023 product builds published yet. No hardware access authorized/performed. User explicitly authorized commit/push and cumulative test releases.

Checkpoint: dependency-only update preserves exact app manifest/image and namespace; newer driver required; no downgrade or equal-version replacement. 180 interrupted dependency-only recoveries plus existing 180 app/driver placement cuts passed normal and ASan/UBSan. Existing staged installer,110 provider cuts,160 app download cuts,65 settings cuts,153 paged settings cuts and feed regression passed. The line81 regression was a scratch-buffer ordering issue: descriptor() overwrote its input before prepareProvision consumed the provider closure. Original prepare-before-descriptor order restored; same-app detection now parses owned manifest JSON independently. Linux GCC existing PackageFiles indentation warnings retained via documented host wrapper; no native source modified.

Still pending: automatic first-start/profile reconciliation, both-media discovery/client integration, product activation, full target/resource/admission builds and publication. Product feed identity/URL parameterization is in progress separately from this checkpoint.

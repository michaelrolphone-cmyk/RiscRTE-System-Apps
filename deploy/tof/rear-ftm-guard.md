# Rear FTM disabled-state overlay

The rear camera can keep publishing video and both LiDAR heads while its firmware
explicitly disables Wi-Fi ranging. The dual-radio coordinator now stops **new rear
FTM requests** for JSON `supported: false`, `enabled: false`, or
`status: "disabled_in_firmware"`. Missing markers and ordinary ranging failures
retain their previous eligibility. Observations are processed and completed
requests are reaped before this guard; a running request is neither cancelled nor
forgotten. Main front scheduling, calibration, and clock checks are unchanged.

## Exact deployed-base candidate

The active ToF image predates parts of current main. A full build from main would
replace unrelated coordinator code. The incident candidate instead copies only
`/app/tof_dual_radio.py` onto the audited local arm64 image:

`sha256:b4046480b31b9648ade0b852d42f7d0fd6de9d228297d1dd205763ff62ca108c`

Its original target-file SHA256 must be
`d2ea6b81a2e60259e7fd6c95364b658a96594bee4ece009d1f00ea9476fc4ad8`.
The source lock continues to describe the repository's normal CI build; it does
**not** claim that the remaining deployed image files match current main.

From the committed checkout, with that exact base already in the local Docker
engine, run:

```sh
python3 deploy/tof/build_rear_ftm_guard.py --output /absolute/new/candidate-directory
```

The recipe never pulls dependencies. It validates committed source/test hashes,
uses a base tag checked against the full image ID, preserves original OCI
provenance, and adds separate patch labels. It verifies that inherited runtime
configuration and all base filesystem layers remain unchanged. The saved archive's
one added layer must contain only the reviewed Python file.

All existing tests in that image and the new behavior tests run in an isolated
network-disabled container without production state/configuration mounts.
Artifacts include the exact Dockerfile, build/test logs, candidate image archive,
and a receipt binding the base, patch commit/tree, file hashes, image ID, and
archive checksum. Reproduction requires the pinned base image; no registry digest
was available for that local image.

Building does not stop, declare, or activate a service and does not change the
active Compose files. Deployment is a separate reviewed image-only override of
the existing `biome-tof` project's `tof` service, preserving its four state/config
mounts, main front binding, private HostIO adapter, and lifecycle/custody procedure.

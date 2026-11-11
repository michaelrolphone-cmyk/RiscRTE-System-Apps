# Workstation maintenance reference

This profile runs the headless `infra_agent` app with `infra.node@1` and its station dependency. It supplies `scheduler.drive`, `service.reconcile`, `service.health.observe`, and `source.poll`. Native HTTP v2 adds authenticated bounded requests while retaining HTTP v1. State and unresolved effect evidence use the independent mutable-data volume. Optional `payload.transform` and network OTA are not offered.

The required specification is `esp32s3-infrastructure-agent-spec-v1`, revision `f0e52f9fb704bd984a1b7ed8d0bdc9c74b2596a875d58ea74b7354f2c6667ac2`. See `CONFORMANCE.md` for evidence and qualification boundaries. The requested maintenance interval is 15 seconds; achieved board timing is **unqualified** until measured.

## Hardware and package

Select only an ESP32-S3 with **16 MiB flash and octal PSRAM** compatible with the 80 MHz profile. CPU is 240 MHz, runtime flash mode QIO, serial writer mode DIO, flash frequency 80 MHz. Actual PSRAM capacity, wiring, module revision, USB port and power characteristics must be recorded during physical qualification. The installer rejects a different reported chip or flash size. This serial flow requires secure boot and flash encryption disabled; it does not configure eFuses. Secrets in this profile are not encrypted at rest.

Layout is `riscrte-paired-appdata-v2`, ABI 2. Sizes below are reserved ranges, not image lengths; actual lengths and SHA-256 values are in `flash-manifest.json`.

| Region | Offset | Reserved bytes | First install | Routine update |
| --- | --- | --- | --- | --- |
| Bootloader | `0x000000` | `0x8000` | Included | Preserved; exact compatibility check |
| Partition table | `0x008000` | `0x1000` | Included | Preserved; exact compatibility check |
| NVS | `0x009000` | `0x6000` | Preserved | Preserved |
| Runtime bank 0 | `0x010000` | `0x260000` | Included | Only if inactive |
| Mutable app-data | `0x270000` | `0x80000` | Private provisioned image, or blocked template | Preserved |
| Boot store bank 0 | `0x2f0000` | `0x510000` | Included | Only if inactive |
| Runtime bank 1 | `0x800000` | `0x260000` | Must be blank | Only if inactive |
| Boot store bank 1 | `0xae0000` | `0x510000` | Must be blank | Only if inactive |
| Boot selector | `0xff0000` | `0x2000` | Included, written last | Alternate selector sector, written last |
| Pair records | `0xff2000` | `0x2000` | Included | Target bank record |

The package includes `firmware.bin`, `bootloader.bin`, `partitions.bin`, `store.bin`, `otadata.bin`, `bank_state.bin`, `appdata-unprovisioned.bin`; separate `elf/default.elf`, `elf/infra-node.elf`, `elf/infra-station.elf`; exact boot manifests/grants in `bootfs/`; nonsecret schemas/catalog/policy fixtures; offline provisioning and serial installer scripts; pinned LittleFS source; and build/test evidence. A merged flash image is not required. No NVS image is distributed. The v1.2 unprovisioned image starts the captive portal. These workstation tools remain optional advanced maintenance tools.

## Verify and configure services

Use Python 3.11 or newer for the supplied scripts, a C11 host compiler for private provisioning, Node.js 22 or newer for service setup, and esptool 5 for serial access. From the extracted bundle directory:

```sh
python scripts/install_infrastructure.py --bundle . verify
```

Verify the release/source publication through its authenticated owner-controlled channel as well as checking hashes. A SHA-256 value alone does not establish publisher authority. Do not run the installer with Python `-O`.

The inspected owner service origins are below. Deploy the matching service changes and migrations before use; an origin being reachable does not prove that the extension has been deployed or its credentials validated.

| Use | Exact endpoint | Domain protocol |
| --- | --- | --- |
| Enrollment, sessions, inventory, assignment/result sync | `https://capabilities-service.cantikash.chatgpt.site/v1/call` | `1.0` |
| Capabilities maintenance | `https://capabilities-service.cantikash.chatgpt.site/v1/maintenance/call` | `maintenance/1` |
| Scheduler maintenance | `https://responsibility-scheduler.cantikash.chatgpt.site/v1/maintenance/call` | `maintenance/1` |

The owner workstation publishes the bundle's exact contract/provider catalog and issues one-use enrollment. Create a private JSON file containing `url` (the Capabilities `/v1/call` endpoint), and the applicable `site_token` and `owner_bridge_token`. Then run:

```sh
umask 077
node scripts/configure_infrastructure.mjs private-service-setup.json fixtures/catalog.json private-enrollment.json
```

This command mutates the service: it publishes pinned catalog definitions and creates an enrollment expiring after one hour. It validates the returned catalog digests and writes the bootstrap and newly generated participant credential only to the private output file. Owner credentials remain on the workstation and must not be installed on the ESP.

Configure each target deployment's `MAINTENANCE_BINDINGS` as a JSON array of records with `binding_id`, `target` (`scheduler` or `capabilities`), `credential_sha256` (SHA-256 of the distinct raw target token), `enabled: true`, `expires_at` (UTC epoch milliseconds), `max_items: 4`, and `max_elapsed_ms: 1000`. Use binding IDs matching the device policy. These scoped tokens authorize bounded target maintenance, not general administration. Apply the migrations from the matching service source, including Capabilities `0009_embedded_infrastructure.sql` and the Scheduler maintenance schema. Keep target tokens distinct from the participant credential. Both services reject missing, expired, wrong-target or mismatched bindings.

If a Site access token is required, the node sends it in `OAI-Sites-Authorization: Bearer …`. Participant and maintenance authorization uses `Authorization: Bearer …`. References for these secrets belong in policy; secret values belong in the private credential table. The app does not follow redirects or accept task-supplied arbitrary destinations.

## Provision an initial private image

Copy `fixtures/node-policy.example.json` to `private-policy.json`. Set the real SSID, service origin, exact target endpoints, credential references, expiry times, and desired bindings. Preserve the generated catalog/provider/package digests from this bundle. Configure real health/source origins and bounded schemas, or remove their bindings, endpoints and enabled providers consistently. Do not use the `.example` endpoints. `maintenance_qualified` records the bounded adapter contract; it does not assert measured physical cadence.

Create a private JSON object mapping each referenced credential name to its raw secret: Wi-Fi password, participant credential from `private-enrollment.json`, separate maintenance tokens, optional observation tokens, and any Site access token. Missing references are prompted without echo. Keep the combined credential table within 4 KiB and each secret within 1024 bytes.

```sh
umask 077
python scripts/provision_infrastructure.py \
  --policy private-policy.json \
  --credentials private-credentials.json \
  --enrollment-id ENROLLMENT_ID_FROM_PRIVATE_OUTPUT \
  --output private-appdata.bin
```

Enter the one-use bootstrap when prompted. The tool generates an installation UUID, frames the bounded state with header/body CRCs, creates the exact 512 KiB LittleFS image using the pinned core, remounts it and checks stored bytes. Secrets are not arguments or public fixture content. Keep the image, credential files and serial backups private. Do not re-provision an existing device as a new installation to hide recovery errors.

## Preflight and first installation

Choose the actual serial port instead of the example below. Put the board in its ROM serial loader. Every serial invocation creates a new private backup directory and reads the full 16 MiB before writing. Preflight logs chip, flash and security information; physical PSRAM and board identity still need installation-time verification.

```sh
python scripts/install_infrastructure.py --bundle . --port /dev/ttyACM0 \
  --backup private-preflight preflight
python scripts/install_infrastructure.py --bundle . --port /dev/ttyACM0 \
  --backup private-first-install --appdata private-appdata.bin first-install
```

The first-install command refuses an incompatible existing partition table, nonblank mutable data or a populated second bank. It verifies each write before activating the pair. It never issues a whole-chip erase. An incompatible existing installation requires an explicit separately reviewed migration; do not bypass the checks. Reset after completion. Check serial diagnostics and participant status for provider admission, storage recovery, Wi-Fi/TLS, a current session, accepted sync, published capacity, and successful required maintenance bindings. Only those healthy conditions permit boot confirmation.

## Update and recovery

Before updating, use authenticated desired state to drain admissions and preserve outstanding evidence; ensure an accepted checkpoint or an explicitly retained unknown outcome. A state schema change needs its own forward/backward compatibility review. This release uses state schema 1 and layout ABI 2; the routine flow requires exact installed bootloader/table compatibility.

```sh
python scripts/install_infrastructure.py --bundle . --port /dev/ttyACM0 \
  --backup private-update update
```

The installer requires the newest selected bank to be confirmed, verifies the installed pair against its record, writes the inactive runtime/store, verifies both, writes the pair record, then switches the selector last. It preserves NVS and all mutable data. It rejects ambiguous selectors, altered images, incompatible layouts and a newer unconfirmed candidate. After reset, the app confirms only after current-boot recovery and critical health checks. The bootloader rollback path remains available when a candidate is not confirmed.

For explicit serial recovery, inspect the private backup and select the known verified bank (`0` or `1`):

```sh
python scripts/install_infrastructure.py --bundle . --port /dev/ttyACM0 \
  --backup private-recovery --bank 0 recover
```

Recovery validates the chosen pair and changes selection metadata only. It neither formats nor replaces app-data. Keep an unresolved effect ledger intact across rollback. Storage corruption, retained handles, an incompatible state schema or an unknown outcome must remain visible for repair. Offline repair can use `provision_infrastructure.py --state DECODED_SNAPSHOT.json --existing-image ORIGINAL_IMAGE.bin` with the existing policy and private credentials; it preserves the supplied pending custody and requires the original volume. This is an explicit repair workflow, not part of routine update.

## Rebuild and software checks

Check out the exact runtime, System Apps and ESP-IDF commits recorded in `flash-manifest.json`, including their pinned dependencies. Use ESP-IDF 6.1 and the Xtensa ESP toolchain recorded in the build evidence. Source that SDK's `export.sh`; install the build script's Python dependencies, including `jsonschema`.

```sh
export RISC_RUNTIME_ROOT=/absolute/path/to/RiscRTE
export IDF_TOOLS_PATH=/absolute/path/to/idf-tools
export INFRA_NATIVE_BUILD=/absolute/path/to/native-build
export INFRA_OUTPUT=/absolute/path/to/new-bundle-directory
bash scripts/build_infrastructure_bundle.sh --evidence /absolute/path/to/evidence
```

Run this build command from the System Apps source checkout, not the extracted binary bundle. It configures the headless profile, builds native firmware with one compiler job, links all three ELFs, generates the pinned fixtures, creates/extracts/checks the SPIFFS store, creates/remounts/checks mutable data, generates the exact range/hash manifest, verifies the install package and produces a ZIP. The output directory must not already exist. Commit the release sources before packaging so manifest source identities describe the built tree.

```sh
RISC_RUNTIME_ROOT=/absolute/path/to/RiscRTE \
CAPABILITIES_SOURCE=/absolute/path/to/capabilities \
SCHEDULER_SOURCE=/absolute/path/to/scheduler \
bash scripts/test_infrastructure.sh
python scripts/test_infrastructure_install_recovery.py
```

Service tests run from each service checkout with `npm test`; build the Scheduler first so its worker integration test uses the current `dist` output. The conformance record identifies runtime transport, storage and admission tests separately. No hardware flash, live enrollment, credential rotation or 72-hour soak is implied by a host test.

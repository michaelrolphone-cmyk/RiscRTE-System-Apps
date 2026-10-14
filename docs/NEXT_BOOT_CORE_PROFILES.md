# Explicit next-boot core profiles

Build the installed-app service with `--native-feed --boot-profiles` and the
product-owned HTTPS feed URL/product. This additive API requires the native
`platform.provision-request@1` provider. Ordinary builds retain the old API and
memory footprint. The product bridge must first admit its complete boot policy,
including any SD image sources and the native request provider; a public feed
cannot invent hardware, grants, namespaces or a new boot policy.

The existing `riscrte.native-packages` / `version:1` envelope gains a
`boot_profiles` object. A selected row contains exactly `native`, `profile`,
`time`, and `mirrors`. `profiles` still contains the selected ordinary-package ID
array (possibly empty). The native metadata binds layout, Runtime version,
store ABI, firmware size and firmware SHA-256. A public profile contains only
`schema`, `schema_version`, `image`, and `files`; Wi-Fi credentials are forbidden.
Image profiles use `{url,bytes,sha256}` and up to160 exact file inventory entries.
Version4 compact file tuples are `[relative_path,bytes,sha256]`. Explicit SNTP
servers come from the product's `riscrte.sntp` time policy, without defaults.

After first-frame promotion, the existing foreground owner acquires its normal
network/settings grants and service lease. The additive API requests the saved
network-tool default profile only when a changed core store needs a native
request. It checks ownership before and after every credential read, never
writes settings, and wipes all credential buffers. The service escapes all credential control bytes directly into its bounded
native request buffer and wipes that buffer after queueing, without heap copies. It does not log or
include credentials in feeds, build receipts or public status.

Native owns persistence and admission. The service queues against the exact
active-store identity and reports a pending boot, not a completed installation.
A matching pending target is idempotent; a different pending target is left
alone. An ambiguous queue result is reconciled by status on the next refresh,
never by an immediate blind write retry. Foreground queueing cannot reset the
device. Work starts during the next ordinary boot; successful native bank
activation retains the established boot-owner restart/rollback procedure.

Optional SD mirrors publish only content-addressed ELF files, never manifests,
grants or a dynamic core identity. The service prefers a supplied generic
storage volume. Missing, exported, full or cleanly failed media falls back to
the complete internal bootfs cohort. Partial files cannot become boot policy.
Interrupted copies retry their immutable paths. Uncertain acquisition, close,
release or owner custody retains the operation and forbids fallback/queueing.
The app/driver loader must enforce the matching admitted fallback policy and
load-time content integrity. T-Watch supplies no SD capability.

No data partition or layout change is required. The core store is downloaded
to the existing inactive bootfs bank; it is not duplicated in the small private
app-data package volume. Ordinary app installations remain independently
journaled, and profile changes do not remove user apps or private data.

## Verification

- `RUNTIME_REPO=... scripts/test_installed_apps_boot_profile.sh`: real service
  with deterministic storage/HTTP/native-boundary doubles; first start, current
  and pending targets, native mismatch, rejected public credentials,160-file
  bound, SD success/clean failure,100 interruption/retry cuts, changed profiles,
  ambiguous native queue and retained acquisition.
- `RUNTIME_REPO=... scripts/test_automatic_foreground.sh`: additive compatibility,
  read-only default credentials, busy retry, per-read/queue ownership fences,
  network/service draining and sticky retained cleanup.
- Both suites passed ASan+UBSan with leak detection disabled only because the
  execution environment uses ptrace. Existing automatic and media suites pass.
- ESP32-S3 service target compile/link passed with only existing permitted
  imports. Host/target-link tests are not hardware qualification.

# Native firmware identity gate

The Wi-Fi product job builds shared application/provider artifacts and runs host Runtime graph and ELF admission. Those checks do not establish which native firmware is installed or published with the companion stores.

Product success additionally requires `native_cohorts` in the reviewed baseline lock, with one entry per product. There are deliberately no entries today. Do not populate them from matching API versions, a host fixture, a product header, or an SDK checkout. The current lifecycle/authority checks remain independent and unchanged.

## Required evidence

Each entry locks a receipt (file, size_bytes, sha256), runtime_commit, product_commit, and artifacts. The receipt must identify schema1, the lane, exact commits, a clean source tree, scope `linked-product-firmware`, full_firmware_linked true, and the two exact baseline companion asset hashes. Its artifact map must match the reviewed lock exactly and contain:

- firmware binary, linked ELF, linker map and link log;
- the actual selected product profile and settings header;
- full firmware compile_commands;
- a measured qualification JSON tied to that exact firmware/ELF, source commits, product and companion stores.

Every artifact is materialized beneath the baseline directory and checked for size and SHA256. The measured qualification must record passed capability_admission, persistent_storage_preservation and resource_budget. These are required results, not permission to create assertions without tests. Before locking a receipt, review the underlying build and qualification logs, including source-selected definitions, native writer bindings, partition/mount identity, real storage reservation and reboot preservation, and actual product heap/stack/link limits. A source profile containing PREFERENCE_STORE=1 alone is insufficient.

An old native image cannot be certified by linking a newer host Runtime. If a genuine successor changes Runtime, the native product must actually be rebuilt and qualified, and the paired app/provider build and host admission must use that same revision. The framework currently requires the pinned Runtime; changing that pin is a separate reviewed cohort change.

## Current evidence limits

Watch source profiles enable preferences but do not enable the native atomic namespace6 lifecycle. Wave native-idf61 and provisioning profiles enable preferences and already register wifi_settings in namespaces1 and6. Wave current provisioning source pins Runtime51520408, while the older released-baseline0.1.48 identifies Runtime4107. Neither establishes that the stores selected here are paired with Runtime712ecc. X4's Wi-Fi successor profile is a separate candidate and requires real firmware linking and resource/storage qualification.

The verifier's positive fixtures are explicitly synthetic schema tests. They are not product receipts. No production lock or receipt is fabricated. The workflow can retain host evidence and then finish with native_identity_pending/error instead of reporting product success. Even a successful offline cohort result does not establish installation, deployment or physical device operation.

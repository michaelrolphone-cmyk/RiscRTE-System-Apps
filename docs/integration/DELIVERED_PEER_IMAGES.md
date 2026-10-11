# Peer Files images for the actually delivered baselines

This delivery supersedes the staging-only X4 upgrade path in PEER_PRESERVING_INCREMENTS.md. Use the new per-device FULL-AND-PRESERVING-TEST archives. The previous combined preserving archive's X4 .82/1108e7b bridge is not the user's .81 upgrade.

Fresh complete16MiB images are available for X4 0.1.83 and Watch1.1.4 at address0x0. They replace internal NVS/settings, AppData and both firmware/store banks. Back up internal user files before using them. X4 includes its matching Apps/fonts/Drivers SD tree; Watch has no SD payload in this composition. Independent image extraction and partition/journal checks pass, and both stores are byte-identical to their qualified peer cohorts. No hardware was accessed or flashed.

The actual delivered X4 0.1.81/aac8650 archive was checked against its original ZIP and receipt: boot SHA92bc02d99223d0f261863e83a944ca559c8ea4440f646574255569947e8945bb. Its dedicated preserving bridge is0.1.82/747b7be on branch feat/shared-files-delivered-081-bridge. Only cohort.json changes in the old store. Native firmware is the qualified Runtime209274b/X47394ea1 composition. Its map and Files KV6 grant already exist, so default preservation admits0.1.83; the unrelated staging V2 migration record remains inert. This distinct bridge is included in the new X4 archive.

The actual delivered Watch1.1.2/e73c91e uses bridge1.1.3/89a2a76 followed by peer1.1.4/e3c24a2. The native-selected V2 record authorizes its exact one-to-eight-file export expansion. Both delivered baseline -> bridge transitions pass the original Runtime81845e8 admission under UBSan with the actual64-app/32-provider build capacities. Both bridge -> peer transitions pass current Runtime209274b, as do unchanged target checks. There were zero hardware/storage callbacks.

Preserving payloads exclude NVS/AppData and require the paired-cohort updater; do not write them at0x0. Candidate hosting URLs are not published. Apply and confirm each bridge before its target. Full initial images are the straightforward installation route when replacing internal data is acceptable.

Native ELF/map files are separately packaged for each device. All four archives are below20MiB, include SHA256SUMS and pass ZIP round-trip verification. Exact image, archive, source and transition evidence is in delivered-peer-evidence/. Each README-FIRST includes installation steps, peer connection steps and all eight Watch paths with namespaces1,2,3,62,64.

Sources: Runtime209274bb65a3bc90d0e6a8f5d69199263c1bc7b6; native X47394ea165deb5fbb568b6993fb7cf6fab766ef24; Watche3c24a2ae3c2df5d440eebf2efab375f036841a6; Driversb2419513da200b150192ba46ffe1b396a82171cc. The implementation and earlier qualification remain in System6199860. The new delivered-X4 bridge is a metadata-only branch over the same native source.

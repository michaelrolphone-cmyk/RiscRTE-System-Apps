# RiscRTE System Apps

Independent source repository for RiscRTE system applications migrated from `michaelrolphone-cmyk/T5S3-Reader`.

## Migration policy

- `T5S3-Reader` is an upstream **read-only** compatibility/source reference during migration.
- System-app source, manifests, build/test automation, release automation, and version tracking belong here.
- App IDs and published versions must remain compatible with the corresponding RiscRTE releases while migration is in progress.
- Source parity is tracked in `system-apps-manifest.json`.

Until the runtime is officially switched to this repository, changes in the upstream `Apps/` tree should be synchronized here without modifying the upstream repository.

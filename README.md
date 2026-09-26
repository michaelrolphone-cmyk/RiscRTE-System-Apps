# RiscRTE System Apps

Independent source repository for foundational RiscRTE system applications migrated from `michaelrolphone-cmyk/T5S3-Reader`.

## Scope

This repository contains applications required for basic operation and first-use system workflows: launching installed apps, installing/updating/managing software, configuring the runtime, and foundational file browsing/dispatch. Optional utilities, productivity tools, developer/debug tools, games, and domain-specific apps belong in their respective repositories.

During migration, `T5S3-Reader` is a strictly read-only upstream compatibility/source reference.

## Application documentation

- [Springboard / Apps](docs/apps/springboard.md) — installed-app discovery, launcher grid, launch handoff, and Home pin persistence.
- [App Store](docs/apps/app_store.md) — release catalog, application install/update, SD package inbox, and uninstall workflow.
- [Settings](docs/apps/settings.md) — front end for the firmware-owned settings model and complex setting-action handoff.
- [File Browser](docs/apps/file_browser.md) — SD/removable-storage browsing, file-handler dispatch, native ELF launch, delete workflow, and SD↔USB file copy.

## Repository tree

```text
Apps/
  app_store.c
  app_store.json
  file_browser.c
  file_browser.json
  settings.c
  settings.json
  springboard.c
  springboard.json

docs/
  apps/
    app_store.md
    file_browser.md
    settings.md
    springboard.md

system-apps-manifest.json
```

## Migration and parity policy

- System-app source, manifests, documentation, build/test automation, release automation, and version tracking belong here.
- App IDs and published versions must remain compatible with corresponding RiscRTE releases while migration is in progress.
- `system-apps-manifest.json` tracks the approved foundational system-app set.
- Every migrated app must have a dedicated documentation page linked above.
- Documentation is derived from actual app source, manifests, ABI headers, and observed implementation behavior; it must not invent future APIs or specifications.
- An app is not parity-complete until source, manifest/version, build/release behavior, and documentation are aligned.

Until the runtime is officially switched to this repository, relevant upstream changes are synchronized here without modifying `T5S3-Reader`.

# Watch 1.0.1 owning-source tags

The immutable component inventory is `watch-1.0.1-components.json`. Its source
commit is the integrated application source, not the later publication-metadata
commit. Native OTA and App Store manifests live under `Apps/native/`; legacy
Reader manifests retain their separate identities and versions.

Publication requires successful default-branch push CI for every configured
workflow at the exact source commit. The tag-only workflow checks default-branch
ancestry and all existing tag targets before creating any missing refs. Existing
lightweight or annotated tags are accepted only when they resolve to that exact
source; they are never moved. No assets, credentials or device state are changed.

The Watch product publishes the configured Watch ELF binaries and full firmware
artifacts separately. These source tags establish owning-repository provenance;
they do not claim that generic build outputs are interchangeable with the Watch
configuration or that hardware qualification was performed.

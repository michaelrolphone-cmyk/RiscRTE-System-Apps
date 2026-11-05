# Durable package media

This checkpoint extends published System 0500c02c with live internal package
installation and update. Build with `scripts/build_installed_apps.py
--internal-packages` to add the explicit native capability dependency; build
`scripts/build_internal_packages.py` for the loader's volume projection.
The native Runtime must enable its internal package volume and boot policy must
select `internal-packages/manifest.json` with software instance 0 and set
`installation.internal_volume_instance` to 0. Preserve the SD instance, catalog
namespace, grant envelope and all existing application/provider selections.
An old firmware without the capability deliberately cannot activate this profile.

`InstalledAppsProvisionV1.h` provides a tagged direct-descriptor entry point for
an explicitly configured catalog client. Downloads, dependency closure, immutable
image admission, manifest activation, updates, removal and private settings use
the selected volume. New downloads prefer SD when the rounded closure plus
headroom fits, otherwise internal storage. Updates keep the original medium.
Protected schema 7 stores media on apps, providers and pending journals; legacy
schemas remain SD. No public descriptor may select a medium or private namespace.

The existing X4 URL in build_portable_updates.py points to the `release-index`
branch, which did not exist when checked. It is a firmware update index, not an
established app/driver descriptor feed. No URL was invented. Automatic App Store
feed discovery still requires a published native descriptor catalog and client
integration. Direct-descriptor tests do not establish automatic online discovery.

Tests: 180 app/driver interruption cuts with real POSIX files, inserted-SD restart,
internal update/removal and namespace persistence; sufficient SD/full SD/both-full/
legacy-volume selection; existing staged/download/provider/settings regression
suites. Native image admission is a fixture boundary here and tested separately
by Runtime host loaders. Target links and imports checked. No full firmware,
LittleFS power-cut or hardware qualification is claimed; delivered .111 is intact.

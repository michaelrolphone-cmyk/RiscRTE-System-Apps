# App Store

Current upstream version: **1.0.6** (minimum firmware **1.3.10**).

Version 1.0.6 preserves the existing release-catalog and SD Inbox behavior documented by the prior implementation and adds compact state-icon signaling: whenever a row already carries an installed, update, or download icon flag, the app also sets `T5_UI_LIST_ICON_COMPACT`. The current UI ABI defines that flag as bit 4. Firmware owns the actual rendered icon size.

Current upstream source identities at `ff08d329489c62af107c906036d0a926f1a0241f`:
- `Apps/app_store.c`: `50177ddbd885a3980cad3045445f28916518eea3`
- `Apps/app_store.json`: `2f09e08c34c732329303a1f9a1a42e3ce124743d`

See the source for the full catalog, install/update, progress, SD Inbox, uninstall, storage, and failure-handling implementation.
# Current X4 source reconciliation — 2026-10-14

The .107 repair at public System 3f7474bbbdaacb7fb196a8c6b74a23017eff2772 is present in the deployed Home ELF (SHA-256 c9754aa1d4d9fcb08e53ed2031bcb34122509ef4f1fa932ff0af9a3e62b07db5), including the later .112 composition. However, the later System source baseline 8a5ef23480b7711e68fd2d72e9a43cad4a2e2ddc did not include that repair. This branch restores the three qualified clock changes on top of T051 checkpoint de3f6bdbcd8fc1e12d419c8ab624803b736a522f, avoiding a regression on a future Home rebuild.

Only the qualified clock patch, regression fixtures, release version and CI routing are added. The richer current adapter, ordered input, App Store and T051 Wi-Fi changes remain. Product boot registration, native Runtime, OBDTest and other deployed binaries are untouched. No claim is made to have recovered d3eb532.

Fresh local verification passes: original 123 realtime scenarios plus reconciliation normally and under UBSan; 20 sparse lifecycle cases and 54 six-face rendered comparisons under UBSan; Wi-Fi workflow normal and UBSan including 15-second scan and 30-second join AGAIN deadlines. Linux CI is enabled for this branch to run ASan/UBSan and Home alarm-loading checks.

Source integration only: no new firmware is built or delivered. Physical sleep/wake testing remains outstanding. The request holds build delivery until 2026-10-14 23:30 UTC.

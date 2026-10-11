# Explicit tagged alarm builds

The native-time Settings and sparse Clock builders accept
`--tagged-alarm-utilities /path/to/Utilities`. It reads the two canonical alarm
headers and license from immutable Utilities
`637e13b0bce62ad49b756bec2468a6271d163fc7`, regardless of that checkout's current
branch or working files. The build stages both headers together, selects
`ALARM_SERVICE_TAGGED_V2`, and requires `alarm.service@2` explicitly. SDK hashes
and the license are recorded in `tagged-alarm-sdk.json` and the package notices.
No v1 table is interpreted as a descriptor and there is no fallback to API1.

This selection requires `--alarm-client` plus the existing native-time Settings
or sparse Clock profile. The explicit packages are Settings1.3.8 and Clock0.3.3.
Absent this option, all existing version and capability selections stay the
same. The complete native Settings API1 ELF is byte-identical to System81f884b8:
236484 bytes, SHA256
`92775b9fe056503950ed6db1d23cedd07575350e399a94ccbd350115683df924`.

Settings' visual-alert selector uses the validated API2 output mask. This avoids
reading the former v1 scalar slot, which is a descriptor tag in the new API.
The actual Settings controller/adapter tests cover the tagged visual profile,
native time editing, QuickActions and retained service failures: 332 cases in
fresh processes across ordinary and ASan/UBSan builds. Six focused API1
regressions also pass. The native Settings target passes the real ELF validator
and import/export checks.

Clock's target composition depends on the separately qualified X4 sleep-owner
update: it must validate the descriptor and consume a successful Light ticket
exactly once after successful native return. This build option does not qualify
that owner or activate a product. All providers and foreground clients in a new
cohort must explicitly agree on API2 before packaging. No delivered image,
product source lock or physical qualification is changed by this checkpoint.

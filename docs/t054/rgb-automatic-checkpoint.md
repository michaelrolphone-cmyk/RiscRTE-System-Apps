# T054 explicit RGB automatic policies

The foreground Contexts automatic boundary now accepts the existing shared encoders for face, sleep mode, idle/deep timers, brightness, radio policy, volume, DND, and notification mode. The controller still enforces its product editor applicability mask before calling these encoders. Automatic operations keep PFA0001 origin; generic legacy puts are not promoted to USER intent.

The shared automatic adapter is capability-selected rather than restricted to X4 idle/RF-only macros. The RGB adapter with low-battery and Contexts paths compiles. 47 automatic-policy/controller cases pass ASan/UBSan; exact compile commands and source hashes are recorded in the attached receipt. Product enablement and target admission remain separate.

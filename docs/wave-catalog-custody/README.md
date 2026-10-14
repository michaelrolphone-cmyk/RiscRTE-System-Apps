# Catalog access after display completion

Source baseline: System 9f44a61 (published Waveshare .37 and .38 Springboard). Native catalog storage safety treats an outstanding display transfer as retained custody. Refresh previously entered immediately; launch prepared its first catalog snapshot before its existing display barrier.

Use the existing bounded cooperative display barrier before either first catalog operation. Keep the guard, second barrier and publication/identity revalidation. Asynchronous rendering, its replaceable mailbox, and normal input polling remain intact. Runtime yields advance the active transfer; this does not replace asynchronous rendering with synchronous presentation.

172 Linux normal/ASan/UBSan cases pass, including delayed ACTIVE completion requiring owner-task yields for refresh and launch at 240x240 and 410x502, stale launch rejection, retained custody fences, and pixel parity. The new delayed-refresh test fails against the original adapter. Independent investigation also reproduced both original paths returning retained with pending display ownership.

This corrects an established catalog/display boundary defect. It does not prove the physical Clock-face-change to Springboard blank screen or the first APPS UNAVAILABLE event has no additional cause. No hardware test or flash was performed. The face atomic grant investigation found no proven cross-handoff leak.

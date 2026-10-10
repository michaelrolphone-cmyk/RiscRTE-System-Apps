# Automatic app stage logs

`--stage-logs` selects direct timestamped diagnostic statements in the shared
adapter. No serial command, event recorder or phase decoder is required. The
ordinary build compiles these calls away. The existing diagnostic Runtime SDK
may still be staged for custody; its trace callback is not called by this mode.

The app prints its first accepted software touch sample and release, meaningful
Home/crown/navigation actions, Quick Controls open/close, Settings/timezone
actions, draw begin/end, display submit, skipped identical pixels and completed
presentation. The timestamp is monotonic milliseconds, not physical contact
time. Runtime prints the named app/provider loading stages separately.

When the optional display metrics descriptor is present, completed-frame logs
also print native damage window, transferred bytes, GPIO calls, transfer start
and end, refresh, BUSY assertion and completion timestamps. Those are provider
observations printed after completion, with the timestamp validity mask intact;
an absent observation is not a measured zero-duration stage. No snapshot or
formatting runs for every finger move or provider status poll.

Radio policy logs distinguish Wi-Fi permission from connection. A missing
record permits Wi-Fi but leaves Bluetooth off; enabling Wi-Fi policy alone does
not request a connection. Bluetooth state is logged only after confirmed
enable/status success. Failed/retained ownership does not authorize another
app-level diagnostic call; Runtime boundary logs and missing completion show
that interrupted operation. No credentials, SSIDs, preferences payloads or
frame pixels are logged.

The existing nonblocking diagnostic transport can drop output when disconnected
or full. The selected native logging build reports its dropped/truncated counts.
These statements intentionally add formatting/output cost, which should be
disabled in a later performance release after the behavior is understood.

`python scripts/test_plain_stage_logs.py` runs actual Clock/Quick Controls and
adapter flows with normal and ASan/UBSan portrait/native providers. It verifies
automatic readable timestamped output, paired draws and completions, one down
and one release per touch, and no command, phase IDs or per-move stream. Existing
paper and Watch tests run without the flag to preserve prior behavior.

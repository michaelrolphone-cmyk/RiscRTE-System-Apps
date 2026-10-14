# T054 shared Settings portability checkpoint

The opt-in atomic Settings controller now selects preference keys according to available controller features instead of requiring the X4 resident-shell profile. RGB time format, sleep mode and alert mode use explicit USER intents. Existing legacy profiles remain unchanged. Checked native custody remains required, and generic key-value puts are still denied in the atomic view. Deep/idle timer key selection is explicit; existing X4 encoding remains unchanged.

The actual RGB adapter/controller passes six cases in normal and ASan/UBSan builds: sleep save, format save, cancel, stale refusal, IO refusal, alert save, with legacy-write fencing and app reentry checks. These provider doubles do not establish device durability; the independent Runtime/Wave SD test covers native durability and returning-media reconciliation.

No product enables this profile yet. Quick Actions and other namespace-1 writers still need migration before Wave namespace 1 may be selected. X4 full-profile regression selection is being reconciled; do not treat the RGB test as X4 qualification. The existing builder still restricts its legacy experimental atomic CLI to the X4 profile; product recipe integration is pending.

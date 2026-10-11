# Home Points readiness

Resident Home 0.3.20 repairs the first foreground Points card. Previously the
adapter treated `ALARM_PENDING` as an ordinary error, then the first draw cleared
the only refresh request. A valid service projection could appear immediately
after that draw while Home continued showing “Points unavailable” until a minute,
battery or input change caused another draw.

The existing-grant adapter now returns a distinct pending result without changing
the caller's output. Foreground Home shows “Loading Points” while reconciliation
is pending. Unavailable/unsupported, valid empty and ordinary error remain
separate states. Pending is a transient presentation status, not a new retained
snapshot status.

Home checks the copied service projection every 250 ms while the foreground
display is settled. The existing alarm pump still owns reconciliation, storage
and RF policy. The readiness check neither pumps the service nor acquires a new
grant, stops Contexts, or performs storage work. If a refresh is in flight, the
check remains due until the display settles. Status or visible catalog changes
schedule the next draw; snapshot/seconds-only churn does not refresh the panel.
The elapsed check is unsigned and survives the millisecond counter wrapping.
New presentation state is committed only after a nonterminal read, and terminal
custody exits before further sampling or rendering.

The retained schema remains 3 and 408 bytes. Its status values, full labels,
landscape geometry, sparse TIMER refresh/expiry behavior and shared resident
shell are unchanged. This repair does not change USB controls or tones.

## Qualification

`scripts/test_home_catalog_adapter.py` covers both service ABI versions, pending
output preservation, ordinary errors and silent retained/output fences.
`scripts/test_home_catalog_render.py` captures actual loading, empty, error,
unavailable and ready Home states plus full-label retained scenes and the codec.
Its utilities revision is now explicitly selectable and recorded, rather than
silently compiling an older catalog header.

`scripts/test_home_points_readiness.py` links the actual Clock, adapter and
selected catalog service. Physical providers are fixtures. Fresh missing
catalog/legacy data uses the service's seven virtual revision-1 defaults;
reconciliation may maintain the occurrence ledger but does not create a catalog
or legacy configuration. An explicitly empty catalog stays empty.

The existing Home reference/controller and sparse Points TIMER suites cover the
shared navigation, completed display image and retained-sleep paths. Normal and
ASan/UBSan renders must match. Host tests and the isolated Xtensa target build
do not establish physical X4 panel timing; device qualification remains pending.

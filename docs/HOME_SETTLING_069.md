# Home settling progress, X4 0.1.69 / Home 0.4.6

The user's 0.1.68 hardware report distinguishes a visible first Home frame from
the following delayed, stuttering physical settling, on boot and Springboard
return. The previous Home content-cache change did not resolve that symptom.

Two independently reproduced defects affect the foreground path:

* `poll_input` treated runtime yield as optional idle time. When foreground work
  consumed the requested poll interval, there could be no yield at all. The
  runtime polls the asynchronous panel through yield; presentation COMPLETE
  does not end the panel's resident settling. The loop now cooperates at least
  once even when its wait budget is exhausted. Input and software painting
  remain independent of physical completion; no frame drain or clean cycle
  was added.
* Home's checked KV GET paused the other background client through the same
  helper as a write. Pause cleared that client's loaded-policy flag. Broadcast
  reads invalidated Contexts, whose reads then invalidated Broadcast. Twenty
  actual client/guard passes produced 100 reads and 100 pauses. GET now retains
  the previous policy-cache validity after a successful checked pause: the same
  case produces 5 reads and 5 pauses. PUT and explicit lifecycle stops still
  invalidate policy. Failed pause still prevents the raw storage read and
  retains native custody. Capture quiescence and model checkpoints remain.

Validation:

* Production client/guard pair, old and corrected read dispatch, normal and
  ASan/UBSan. Covers read reuse, external write invalidation, Broadcast's
  one-second refresh, and failed-quiesce storage fencing.
* Actual System adapter + production Runtime + selected panel 0.1.16, with
  deterministic GPIO/BUSY/SPI timing. A 25 ms foreground-work cost after first
  presentation completion reproduces .68's stuck settling after 3,000 polls.
  With the fix, settling completes at 1/8/20/50 ms app poll intervals, with
  bounded provider slices and continuing touch sampling. At a 20 ms poll
  interval, this model completes in 2,711 ms; the panel retains its intentional
  2,300 ms settling envelope. These are model timings, not device measurements.
* The same cadence matrix without injected foreground cost passes. Normal and
  sanitized actual Home transition endpoints and 14 asynchronous resident
  sleep/settle/cancellation cases pass. Sparse Home storage guard and Broadcast
  client checks pass. The target Xtensa Home builds and passes structural checks.
* The older standalone Contexts client fixture fails at line 93, expecting
  time-based policy reload after a raw unguarded write. The identical failure
  is reproduced from untouched .68 sources; its client header and test are
  unchanged here. It is not reported as passing. The new paired-client test
  exercises the shipping guarded-write invalidation path.

Only Home is rebuilt for this product test. Contexts 0.4.0, scene-host 0.3.0,
Lists 0.1.1, Springboard 1.7.30, panel 0.1.16 and qualified native Runtime 0.2.3
remain byte-identical. Physical behavior of 0.1.69 awaits device testing.

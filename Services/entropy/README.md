# Copied entropy facade

`crypto-entropy` is a development-only ordinary provider exposing
`crypto.entropy@1`. Its sole dependency is `platform.entropy@1`, copied from
Runtime source a3d0b24af7398ef6d004a7199d043b9779f4d829. It makes no native call
at activation. An explicit fill copies 1–32 hardware-backed bytes through its
own scratch space. Failures leave the caller's output unchanged; scratch is
wiped through volatile stores. An unavailable entropy source has no fallback.

The caller owns session/credential policy; this provider stores neither. It
cannot enable Wi-Fi, alter RF ownership, create persistent keys or open sockets.
The native backend must establish its RF readiness and concurrency lease before
success. Activation tokens reject stale copied tables. Reentry and quiesce
while filling are rejected. Native CONTEXT, RETAINED and unknown status latch a
terminal fence, preventing further native I/O and successful unload.

Run `bash scripts/test_entropy_service.sh`, optionally with `SANITIZE=1` and
`ASAN_OPTIONS=detect_leaks=0` under ptrace. The nine actual-provider cases cover
copied-output failure, reentry, prior activation, retained/context failures,
unknown status, malformed dependency and counter exhaustion. Run
`scripts/build_entropy_service.py` with `NATIVE_APP_CC` or `PLATFORMIO_CORE_DIR`
for Xtensa ELF validation, contract hashing, bounded imports/BSS/stack and the
source/output receipt. This is not device qualification or product activation.

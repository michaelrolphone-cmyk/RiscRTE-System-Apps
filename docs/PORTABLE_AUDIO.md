# Explicit foreground audio lifecycle, version1

`PORTABLE_AUDIO_SESSION` is a build-only opt-in for applications that provide
the two hooks in `PortableAudioSession.h`. It requires `PORTABLE_ALARM_CLIENT`.
These are hidden app-local links, not Runtime exports, registry capabilities,
audio drivers, tasks or a second audio stack. Builds without the define retain
their existing behavior and package versions.

The application owns its audio grant and attempted stream. Its suspend hook
must stop only that stream, be idempotent after successful close, and never
restart playback/capture. An uncertain cleanup returns false without repeated
hardware calls. The adapter retains the invocation instead of calling alarm
storage/output methods, releasing grants or handing off to another app.

The foreground alarm path reads memory-only status before a service step. An
existing occurrence preempts audio before stepping. A new occurrence exposed by
durable VERIFY_OCC preempts immediately after the step, before the subsequent
ACTIVATE_RTC/START_AUDIO phases can open the shared speaker. The same check is
used during initial reconciliation and modal pumping. This depends on the
reviewed ordinary alarm-service contract and is tested with its source.

Sleep closes audio before touch is suspended or the alarm sleep preparation is
called. Root Back closes it before requesting a return launch. UI failures close
before app_main can return because Runtime's exit barrier precedes app fini.
Native-retained sleep preserves the existing immediate-return/no-late-I/O path.
After an alarm or wake the app remains stopped until a fresh user Start action.

Runtime0.1.12's reviewed audio prerequisite is required for deployments: healthy
I2S streams must not revoke bound alarm storage, but any live I2S token still
blocks sleep and app unload. Failed/closing I2S must continue to block provider
storage. No app hook or metadata minimum can substitute for that invariant.

`python scripts/test_portable_audio.py` runs normal and ASan/UBSan fixtures for
preexisting and newly exposed alerts, exact dismissal/retry, simultaneous due
events, display/navigation failure, bounded stop-only, failed app-audio cleanup,
normal Back and native-retained sleep. Existing alarm/Wi-Fi suites remain
required. Tests simulate providers; no physical audio or timing qualification is
claimed.

`PORTABLE_AUDIO_CONTINUOUS_CAPTURE` enables the additional app-local
`portable_audio_capture_active()` hook. A requested active capture suppresses
ordinary touch-inactivity sleep. Explicit stop restores the usual idle policy;
alarm, exit and failed-cleanup paths retain their existing behavior. The opt-in
is intended for continuous microphone monitoring. Other audio apps are unchanged.
The adapter fixture tests twenty idle deadlines in plain/Nova and sanitizer builds.

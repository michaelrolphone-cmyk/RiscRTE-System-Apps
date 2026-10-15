#!/usr/bin/env python3
"""Focused opt-in async Watch canvas regressions over the production adapter.

No release artifacts or frozen recovery candidates are touched. Healthy ACTIVE
presentation ownership is distinct from a mutable provider lease. Failed status
must fence all later normal I/O and preserve invocation allocations and grants.
"""
import os
import sys
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/watch-async-canvas'
CORE = [
    'clock-monotonic', 'clock-missing', 'clock-truncated', 'clock-failure',
    'stage-profile-bounded', 'stage-profile-short', 'stage-profile-retained', 'stage-profile-clock-failure',
    'poll-wait-provider-cost', 'poll-wait-provider-cost-wrap',
    'raster-pump', 'raster-pump-complete', 'raster-pump-failure',
    'touch-terminal', 'ordered-taps-0', 'ordered-taps-16', 'ordered-taps-2300',
    'canvas-allocation-failure', 'mailbox', 'mid-paint', 'damage', 'fini-pending', 'fini-drawing', 'crown',
    'launch', 'handoff', 'submit-rejected', 'invalid-surface',
    'fallback-no-async', 'fallback-retains-image', 'fallback-no-retain',
    'fallback-short-runtime', 'failure-after-yield', 'failure-fini-yield',
    'failure-crown-yield', 'status-false',
    'status-failed', 'status-superseded', 'status-unknown', 'timeout',
    'timeout-wrap', 'zero-token', 'retain-refused',
]
MODAL = [
    'idle-repainting', 'idle-contact-drain', 'idle-crown-drain', 'healthy-poll', 'controls', 'controls-settings', 'controls-save', 'torch-alarm', 'alarm',
    'crown-controls', 'crown-alarm', 'crown-loading', 'crown-cue',
    'crown-alarm-stop-failure', 'wifi-handoff', 'failure-poll',
    'failure-controls', 'failure-alarm', 'failure-loading', 'failure-cue',
]
CONTEXTS = [
    'healthy-poll', 'contexts-paint', 'failure-poll', 'failure-controls', 'failure-alarm',
    'failure-loading', 'failure-cue', 'failure-context-message', 'failure-context-message-entry', 'failure-context-launch',
]


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    common = [*shlex.split(os.environ.get('CC', 'cc')), '-std=c11', '-O1', '-g',
              '-Wall', '-Wextra', '-Werror',
              '-I' + str(ROOT / 'lib/PortableApps/include'),
              '-I' + str(ROOT / 'lib/NativeApps/include')]
    failures, runs = [], 0
    sanitizer_env = os.environ.copy()
    # Do not suppress LeakSanitizer on hosts that can run it. Only the known
    # ptrace restriction permits retry with leak detection disabled.
    def run_case(binary, case, sanitized):
        nonlocal runs
        env = sanitizer_env if sanitized else None
        result = subprocess.run([str(binary), case], text=True, capture_output=True,
                                timeout=30, env=env)
        if sanitized and result.returncode and 'LeakSanitizer does not work under ptrace' in result.stderr:
            sanitizer_env['ASAN_OPTIONS'] = sanitizer_env.get('ASAN_OPTIONS', '') + ':detect_leaks=0'
            print('LeakSanitizer is blocked by ptrace; ASan/UBSan remain enabled.', flush=True)
            result = subprocess.run([str(binary), case], text=True, capture_output=True,
                                    timeout=30, env=sanitizer_env)
        runs += 1
        print(result.stdout, end='', flush=True)
        if result.returncode:
            print(result.stderr, end='', flush=True)
            failures.append(f'{binary.name}:{case} (exit {result.returncode})')

    for sanitized in (False, True):
        tag = 'san' if sanitized else 'normal'
        san = ['-fsanitize=' + os.environ.get('WATCH_TEST_SANITIZERS', 'address,undefined'), '-fno-sanitize-recover=all',
               '-fno-omit-frame-pointer'] + (['-no-pie'] if sys.platform != 'darwin' else []) if sanitized else []
        reducer = OUT / f'touch-{tag}'
        subprocess.run([*common, *san, str(ROOT / 'test/native_apps/portable_touch_ordered_test.c'),
                        '-o', str(reducer)], check=True, timeout=120)
        subprocess.run([str(reducer)], check=True, timeout=30, env=sanitizer_env)
        core = OUT / f'core-{tag}'
        subprocess.run([*common, *san, '-DPORTABLE_ASYNC_RGB565', '-DPORTABLE_STAGE_PROFILE',
                        '-DPORTABLE_RETAINED_RGB565_HANDOFF',
                        str(ROOT / 'test/native_apps/watch_async_canvas_test.c'),
                        '-o', str(core)], check=True, timeout=120)
        for case in CORE:
            run_case(core, case, sanitized)
        for contexts, cases in ((False, MODAL), (True, CONTEXTS)):
            flags = ['-DPORTABLE_CONTEXTS_CLIENT'] if contexts else []
            session = OUT / f'quick-session-{int(contexts)}-{tag}.o'
            subprocess.run([*common, *san, *flags, '-c',
                            str(ROOT / 'lib/PortableApps/src/quick_session.c'),
                            '-o', str(session)], check=True, timeout=120)
            for rotation in (0, 180):
                binary = OUT / f'{"contexts" if contexts else "modal"}-{rotation}-{tag}'
                subprocess.run([*common, *san, '-DPORTABLE_ASYNC_RGB565', '-DPORTABLE_NOVA_UI',
                                f'-DPORTABLE_TOUCH_ROTATION={rotation}', *flags,
                                str(ROOT / 'Apps/settings.c'),
                                str(ROOT / 'test/native_apps/watch_async_modal_test.c'),
                                str(ROOT / 'lib/PortableApps/src/quick_actions.c'),
                                str(ROOT / 'lib/PortableApps/src/quick_render.c'),
                                str(session), '-o', str(binary)], check=True, timeout=120)
                for case in cases:
                    run_case(binary, case, sanitized)
    if failures:
        raise SystemExit(f'{len(failures)}/{runs} async cases failed:\n' + '\n'.join(failures))
    print(f"Watch async canvas: {runs} real-adapter cases passed in normal/{os.environ.get('WATCH_TEST_SANITIZERS', 'address,undefined')} modes")


if __name__ == '__main__':
    main()

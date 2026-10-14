#!/usr/bin/env python3
"""Run the exact pinned Utilities Stopwatch against the real async adapter.

Reads immutable Git objects only; never modifies the Utilities checkout.
"""
import os
import sys
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PIN = 'd03c6564814ff1588bf73af45b3f0288908466c3'


def main():
    location = os.environ.get('UTILITIES_SOURCE')
    if not location:
        raise SystemExit('UTILITIES_SOURCE must name a Utilities checkout at ' + PIN)
    utilities = Path(location).resolve()
    if not utilities.is_dir():
        raise SystemExit('UTILITIES_SOURCE directory is absent: ' + str(utilities))
    actual = subprocess.check_output(['git', '-C', str(utilities), 'rev-parse', 'HEAD'], text=True).strip()
    if actual != PIN:
        raise SystemExit(f'UTILITIES_SOURCE HEAD must be {PIN}; found {actual}')
    out = ROOT / 'build/watch-async-stopwatch'
    source = out / PIN
    source.mkdir(parents=True, exist_ok=True)
    for name in ('stopwatch.c', 'stopwatch_core.h', 'daily_draw.h', 'utility_frame.h'):
        data = subprocess.check_output(['git', '-C', str(utilities), 'show', f'{PIN}:Apps/{name}'])
        (source / name).write_bytes(data)
    common = [*shlex.split(os.environ.get('CC', 'cc')), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
              '-DPORTABLE_NOVA_UI', '-I' + str(ROOT / 'lib/PortableApps/include'),
              '-I' + str(ROOT / 'lib/NativeApps/include')]
    env = os.environ.copy()
    count = 0
    for sanitized in (False, True):
        flags = ['-fsanitize=' + os.environ.get('WATCH_TEST_SANITIZERS', 'address,undefined'), '-fno-sanitize-recover=all', '-fno-omit-frame-pointer'] + (['-no-pie'] if sys.platform != 'darwin' else []) if sanitized else []
        app = out / f'stopwatch-{int(sanitized)}.o'
        subprocess.run([*common, *flags, '-DPORTABLE_ALARM_CLIENT', '-Dapp_main=watch_test_stopwatch_main',
                        '-Drisc_runtime_get_api=watch_test_runtime_get_api', '-c', str(source / 'stopwatch.c'), '-o', str(app)], check=True, timeout=120)
        for rotation in (0, 180):
            binary = out / f'adapter-{rotation}-{int(sanitized)}'
            subprocess.run([*common, *flags, '-DPORTABLE_ASYNC_RGB565', '-DWATCH_ASYNC_STOPWATCH_FIXTURE',
                            f'-DPORTABLE_TOUCH_ROTATION={rotation}', str(ROOT / 'Apps/settings.c'),
                            str(ROOT / 'test/native_apps/watch_async_modal_test.c'),
                            *[str(ROOT / 'lib/PortableApps/src' / name) for name in ('quick_actions.c', 'quick_render.c', 'quick_session.c')],
                            str(app), '-o', str(binary)], check=True, timeout=120)
            for case in ('stopwatch-crown-retain', 'stopwatch-paint-retain'):
                result = subprocess.run([str(binary), case], text=True, capture_output=True, timeout=30, env=env)
                if sanitized and result.returncode and 'LeakSanitizer does not work under ptrace' in result.stderr:
                    env['ASAN_OPTIONS'] = env.get('ASAN_OPTIONS', '') + ':detect_leaks=0'
                    print('LeakSanitizer is blocked by ptrace; ASan/UBSan remain enabled.', flush=True)
                    result = subprocess.run([str(binary), case], text=True, capture_output=True, timeout=30, env=env)
                print(result.stdout, end='', flush=True)
                if result.returncode:
                    print(result.stderr, end='', flush=True)
                    raise SystemExit(result.returncode)
                count += 1
    print(f'Pinned Stopwatch {PIN}: {count} real-app async retention cases passed')


if __name__ == '__main__':
    main()

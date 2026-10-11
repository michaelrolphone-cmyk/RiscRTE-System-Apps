#!/usr/bin/env python3
"""Assert real Watch Settings input cadence under modeled display latency."""
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/watch-list-cadence'
OUT.mkdir(parents=True, exist_ok=True)
proof = {
    'source': subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip(),
    'dirty': bool(subprocess.check_output(['git', '-C', str(ROOT), 'status', '--porcelain'], text=True)),
    'scope': 'Real Settings/controller/adapter; synthetic wall-clock input and display latency; no target hardware measurements',
    'observations': {},
}
for asynchronous in (False, True):
    for sanitized in (False, True):
        name = ('async' if asynchronous else 'legacy') + ('-san' if sanitized else '')
        binary = OUT / name
        command = [os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                   '-DPORTABLE_NOVA_UI', '-DPORTABLE_TOUCH_ROTATION=0',
                   '-I'+str(ROOT / 'lib/PortableApps/include'), '-I'+str(ROOT / 'lib/NativeApps/include')]
        if asynchronous:
            command += ['-DPORTABLE_ASYNC_RGB565']
        if sanitized:
            command += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie']
        command += [str(ROOT / 'Apps/settings.c'), str(ROOT / 'test/native_apps/watch_list_cadence_probe.c'), '-o', str(binary)]
        subprocess.run(command, check=True, timeout=120)
        rows = []
        for latency, duration in [(n, 400) for n in (0, 16, 40, 80, 240)] + [(240, 80)]:
            row = json.loads(subprocess.check_output([str(binary), str(latency), str(duration)], text=True,
                                                     env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0'), timeout=20))
            if asynchronous:
                assert row['longest_logical_poll_ms'] <= 20, row
                assert row['max_scroll_pixels'] >= 119, row
                assert row['scroll_state_changes'] >= (19 if duration == 400 else 4), row
            rows.append(row)
        if not asynchronous:
            assert rows[4]['longest_logical_poll_ms'] > 200, 'Control must reproduce the physical wait'
        proof['observations'][name] = rows
(OUT / 'cadence.json').write_text(json.dumps(proof, indent=2)+'\n')
print(json.dumps(proof, indent=2))

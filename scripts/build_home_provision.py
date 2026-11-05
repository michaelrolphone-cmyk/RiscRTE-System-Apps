#!/usr/bin/env python3
"""Reproduce the opt-in full X4 Home provisioning target with explicit inputs.

The current delivered Home recipe uses GCC8; Runtime's GCC15 build is separate.
No downloaded SDK, immutable release-input modification or device access.
"""
import argparse, json, os, shlex, shutil, subprocess, sys
from pathlib import Path
from native_toolchain import compiler_path
ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
for name in ('runtime', 'x4', 'sdk', 'utilities', 'reader-scripts', 'output'):
    p.add_argument('--' + name, type=Path, required=True)
p.add_argument('--compiler', help='Explicit ESP32-S3 compiler; otherwise NATIVE_APP_CC/discovery')
a = p.parse_args()
for name in ('runtime', 'x4', 'sdk', 'utilities', 'reader_scripts', 'output'):
    setattr(a, name, getattr(a, name).resolve())
cc = Path(compiler_path(a.compiler))
out = a.output; out.mkdir(parents=True, exist_ok=True)
sdk = out / 'sdk'; shutil.copytree(a.sdk, sdk, dirs_exist_ok=True)
shutil.copytree(a.x4 / 'minimal/interfaces', sdk, dirs_exist_ok=True)
wrapper = out / 'ccbin/xtensa-esp32s3-elf-gcc'; wrapper.parent.mkdir(exist_ok=True)
wrapper.write_text('#!/bin/sh\nexec ' + shlex.quote(str(cc)) + ' -DPORTABLE_ALARM_BUSY_DEFER "$@"\n')
wrapper.chmod(0o755)
for name in ('nm', 'objdump', 'readelf'):
    target = wrapper.with_name(wrapper.name.replace('gcc', name))
    wanted = cc.with_name(cc.name.replace('gcc', name))
    if target.is_symlink() and target.resolve() != wanted.resolve(): target.unlink()
    if not target.exists(): target.symlink_to(wanted)
flags = ['--desk-clock', '--sparse-start', '--desk-lock-home', '--desk-points-face', '--navigation',
    '--alarm-client', '--wake-light-restore', '--display-rotation', '90',
    '--local-sleep-source', a.x4 / 'minimal/apps/portable_sleep.c', '--sleep-capability', 'x4.power',
    '--sleep-sdk', sdk, '--retained-wake-sdk', a.runtime / 'sdk/app', '--tagged-alarm-utilities', a.utilities,
    '--paper-crossfade', '--paper-display-sdk', sdk, '--paper-transitions', '--quick-actions',
    '--quick-radios', '--quick-usb-transfer', '--resident-shell-host', '--resident-runtime-sdk', a.runtime / 'sdk/app',
    '--ble-broadcast', '--x4-idle-source', a.x4 / 'minimal/apps/portable_idle_sleep.c', '--x4-idle-sdk', sdk,
    '--x4-idle-runtime-sdk', a.runtime / 'sdk/driver', '--contexts-rf-only', '--resident-legacy-handoff',
    '--resident-policy', '--resident-loading-catalog', a.x4 / 'minimal/sd-app-catalog.json', '--raster-snapshot',
    '--frontlight-tone', '--frontlight-tone-sdk', sdk, '--display-settled-sdk', sdk, '--crash-report-sd',
    '--boot-animation', '--relocation-tools', a.reader_scripts, '--stage-logs', '--installed-catalog-volume-instance',
    '0', '--deferred-foreground-services', '--experimental-atomic-quick', '--experimental-atomic-home-automatic',
    '--experimental-settings-availability', '--automatic-provision']
command = list(map(str, [sys.executable, ROOT / 'scripts/build_paper_clock.py', *flags, '--output-dir', out / 'home']))
(out / 'command.json').write_text(json.dumps({'command': command, 'compiler': str(cc),
    'extra_define': 'PORTABLE_ALARM_BUSY_DEFER'}, indent=2) + '\n')
subprocess.run(command, check=True, env=dict(os.environ, NATIVE_APP_CC=str(wrapper)))
path = out / 'home/build-evidence.json'; evidence = json.loads(path.read_text())
extra = '-DPORTABLE_ALARM_BUSY_DEFER'
if extra not in evidence['build_defines']: evidence['build_defines'].append(extra)
evidence['compiler_wrapper_defines'] = [extra]
path.write_text(json.dumps(evidence, indent=2) + '\n')

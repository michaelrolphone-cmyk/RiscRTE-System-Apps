#!/usr/bin/env python3
"""Portable Settings semantic routes, full zone directory, and event fencing."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
from test_settings_scene_app import production_validator
ROOT = Path(__file__).resolve().parents[1]
def run(runtime):
    includes = [ROOT / 'lib/PortableApps/include', ROOT / 'sdk/app', runtime,
                runtime / 'sdk/app', runtime / 'sdk/driver', runtime / 'src']
    with tempfile.TemporaryDirectory(prefix='settings-scene-') as directory:
        exe = Path(directory) / 'test'
        sources = ['PortableSettingsScene.c', 'PortableSettingsModel.c', 'PortableSetTime.c',
                   'PortableTimeZone.c', 'PortableTimeZoneCatalog.c', 'PortableTimeZonePreference.c']
        command = [os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                   '-ffunction-sections', '-fdata-sections', '-fsanitize=undefined', '-fno-sanitize-recover=all']
        command += ['-I' + str(path) for path in includes]
        command += [str(ROOT / 'lib/PortableApps/src' / name) for name in sources]
        command += [str(ROOT / 'test/native_apps/portable_settings_scene_test.c'), str(production_validator(directory)), '-Wl,--gc-sections', '-o', str(exe)]
        subprocess.run(command, check=True)
        subprocess.run([str(exe)], check=True)
if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--runtime', '--runtime-sdk', dest='runtime', required=True, type=Path)
    run(parser.parse_args().runtime.resolve())

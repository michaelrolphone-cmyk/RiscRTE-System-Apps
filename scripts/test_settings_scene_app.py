#!/usr/bin/env python3
"""Real Settings app/model/view/client with adversarial Runtime endpoints."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]

def production_validator(directory):
    """Compile exact pure validation functions from the production host source.

    No mirrored validation policy: changes to host/component validation are
    picked up on every run. Raster/provider initialization is outside this test.
    """
    import re
    host = (ROOT / 'Services/scene_host/host.c').read_text()
    components = (ROOT / 'Services/scene_host/components.inc').read_text()
    telemetry = (ROOT / 'Services/scene_host/telemetry.inc').read_text()
    def function(source, name):
        match = re.search(r'static [^\n;]+\b' + name + r'\([^;]*?\)\s*\{', source)
        if not match:
            raise ValueError('Production validation function missing: ' + name)
        start = match.start()
        brace = source.index('{', match.start())
        depth = 1
        end = brace + 1
        while depth:
            depth += (source[end] == '{') - (source[end] == '}')
            end += 1
        return source[start:end]
    code = '#include "RiscScenePointerV1.h"\n#include <string.h>\n#define SCENE_HOST_TELEMETRY 1\n'
    code += '#include "' + str(ROOT / 'Services/text_input/SceneKeyboardV1.h') + '"\n'
    code += 'static bool pointer_enabled,hold_enabled;\n'
    code += function(host, 'terminated') + '\n'
    code += function(host, 'route_index') + '\n'
    code += function(telemetry, 'tm_kind') + '\n'
    code += function(components, 'component_valid') + '\n'
    code += function(host, 'valid_document') + '\n'
    code += 'bool settings_scene_production_valid(const risc_components_document_v1 *d){return valid_document(d,true); }\n'
    path = Path(directory) / 'production_scene_validation.c'
    path.write_text(code)
    return path

def run(runtime, alarm):
    includes = [ROOT / 'lib/PortableApps/include', ROOT / 'sdk/app', runtime,
                runtime / 'sdk/app', runtime / 'sdk/driver', runtime / 'src', alarm]
    with tempfile.TemporaryDirectory(prefix='settings-app-') as directory:
        exe = Path(directory) / 'test'
        sources = ['PortableSettingsScene.c', 'PortableSettingsModel.c', 'PortableSetTime.c',
                   'PortableTimeZone.c', 'PortableTimeZoneCatalog.c', 'PortableTimeZonePreference.c']
        command = [os.environ.get('CC', 'cc'), '-std=c11','-DPORTABLE_DESK_POINTS_FACE', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                   '-DALARM_SERVICE_TAGGED_V2=1', '-ffunction-sections', '-fdata-sections',
                   '-fsanitize=undefined', '-fno-sanitize-recover=all']
        command += ['-I' + str(path) for path in includes]
        command += [str(ROOT / 'lib/PortableApps/src' / name) for name in sources]
        command += [str(ROOT / 'test/native_apps/settings_scene_app_test.c'), str(production_validator(directory)), '-Wl,--gc-sections', '-o', str(exe)]
        subprocess.run(command, check=True)
        for scenario in range(17):
            subprocess.run([str(exe), str(scenario)], check=True, timeout=10)
if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--runtime-sdk', required=True, type=Path)
    parser.add_argument('--alarm-include', required=True, type=Path)
    args = parser.parse_args()
    run(args.runtime_sdk.resolve(), args.alarm_include.resolve())

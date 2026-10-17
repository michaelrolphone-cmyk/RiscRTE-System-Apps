#!/usr/bin/env python3
"""Compare tested host idle conditions with the actual full target recipe."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def function(text, name):
    match = re.search(r'\b'+re.escape(name)+r'\(void\)\s*\{', text)
    if not match:
        raise ValueError('Missing function: '+name)
    start = text.rfind('\n', 0, match.start())+1
    depth, i = 1, match.end()
    while depth:
        depth += (text[i] == '{') - (text[i] == '}')
        i += 1
    return text[start:i]


def idle_guard(text):
    start = text.index('if(!failed && (uint32_t)(millis_now()-last_activity)>=portable_idle_ms()')
    return text[start:text.index('{', start)]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--idle', type=Path, required=True)
    p.add_argument('--app', type=Path, required=True)
    a = p.parse_args()
    recipe = json.loads((a.app/'command.json').read_text())
    build = json.loads((a.app/'wifi-build-record.json').read_text())
    host = json.loads((a.idle/'receipt.json').read_text())
    if host.get('status') != 'PASS' or len(host.get('tests', [])) != 8:
        raise ValueError('All eight actual-controller idle controls are required')
    flags = [v for i, v in enumerate(recipe) if v.startswith(('-D', '-I'))
             or v == '-include' or (i and recipe[i-1] == '-include')]
    commands = []
    def preprocess(source):
        cmd = [recipe[0], '-std=c11', '-E', '-P', *flags, str(source)]
        commands.append(cmd)
        return subprocess.check_output(cmd, text=True, timeout=120)
    adapter = preprocess(ROOT/'lib/PortableApps/src/adapter.c')
    controller = preprocess(ROOT/'Apps/wifi_settings.c')
    command = [v for v in host['preprocessor_command'] if v != '-dM']
    command.insert(command.index('-E')+1, '-P')
    commands.append(command)
    tested = subprocess.check_output(command, text=True, timeout=120)
    result = {'source_commit': build['repository_commit'],
              'recipe_sha256': host['recipe_sha256'],
              'target_command_sha256': hashlib.sha256((a.app/'command.json').read_bytes()).hexdigest(),
              'idle_receipt_sha256': hashlib.sha256((a.idle/'receipt.json').read_bytes()).hexdigest(),
              'commands': commands,
              'target_idle_guard': idle_guard(adapter), 'host_idle_guard': idle_guard(tested),
              'target_wifi_idle_ready': function(controller, 'portable_wifi_idle_ready'),
              'host_wifi_idle_ready': function(tested, 'portable_wifi_idle_ready'),
              'scope': 'Actual target preprocessing matches the host-tested automatic-idle guard and Wi-Fi readiness body. Full target compilation remains separate from synthetic-provider behavior. Hardware not tested.'}
    normalize = lambda s: re.sub(r'\s+', '', s)
    result['idle_guard_equal'] = normalize(result['target_idle_guard']) == normalize(result['host_idle_guard'])
    result['wifi_idle_ready_equal'] = normalize(result['target_wifi_idle_ready']) == normalize(result['host_wifi_idle_ready'])
    (a.idle/'target-condition-verification.json').write_text(json.dumps(result, indent=2)+'\n')
    if not result['idle_guard_equal'] or not result['wifi_idle_ready_equal']:
        raise ValueError('Host idle witness differs from actual full target recipe')
    print('Actual full target and host automatic-idle guard and Wi-Fi readiness body match.')


if __name__ == '__main__':
    main()

"""Explicit tagged alarm SDK selection for a native-time deployment."""
import hashlib
import json
import subprocess
from pathlib import Path

UTILITIES_COMMIT = '637e13b0bce62ad49b756bec2468a6271d163fc7'
HEADERS = ('AlarmServiceV1.h', 'AlarmServiceV2.h')

def options(parser):
    parser.add_argument('--tagged-alarm-utilities', type=Path,
                        help='Read the pinned alarm.service@2 SDK from this Utilities Git checkout')

def stage(args, parser, out, includes):
    repo = getattr(args, 'tagged_alarm_utilities', None)
    if repo is None:
        return None
    if not args.alarm_client:
        parser.error('--tagged-alarm-utilities requires --alarm-client')
    try:
        source = {name: subprocess.check_output(['git', '-C', str(repo), 'show',
            UTILITIES_COMMIT + ':' + ('LICENSE' if name == 'LICENSE' else
            'lib/Alarm/include/' + name)], stderr=subprocess.PIPE)
            for name in (*HEADERS, 'LICENSE')}
    except (OSError, subprocess.CalledProcessError) as error:
        parser.error('Cannot read pinned tagged alarm SDK: ' + str(error))
    for name in HEADERS:
        (includes / name).write_bytes(source[name])
    receipt = {'repository': 'michaelrolphone-cmyk/RiscRTE-Utilities',
               'commit': UTILITIES_COMMIT, 'capability': 'alarm.service', 'api': 2,
               'sha256': {name: hashlib.sha256(data).hexdigest()
                          for name, data in source.items()}}
    notices = out / 'licenses' / 'tagged-alarm'
    notices.mkdir(parents=True, exist_ok=True)
    (notices / 'LICENSE.txt').write_bytes(source['LICENSE'])
    (notices / 'SOURCES.json').write_text(json.dumps(receipt, indent=2) + '\n')
    (out / 'tagged-alarm-sdk.json').write_text(json.dumps(receipt, indent=2) + '\n')
    return receipt

"""Explicit tagged alarm SDK selection for a native-time deployment."""
import hashlib
import json
import subprocess
from pathlib import Path

UTILITIES_COMMIT = '637e13b0bce62ad49b756bec2468a6271d163fc7'
HEADERS = ('AlarmServiceV1.h', 'AlarmServiceV2.h')
CATALOG_COMMIT = '9bd572791a8304194ceb2b7542fc9cbd124e911b'
CATALOG_HEADERS = ('PointsCatalogProjection.h', 'PointsServiceProjection.h')
POINTS_HEADERS = ('AlarmRecords.h', 'PointsRecords.h', 'PointsSchedule.h', 'PointsUtcSchedule.h')

def options(parser):
    parser.add_argument('--tagged-alarm-utilities', type=Path,
                        help='Read the pinned alarm.service@2 SDK from this Utilities Git checkout')

def stage(args, parser, out, includes):
    repo = getattr(args, 'tagged_alarm_utilities', None)
    if repo is None:
        return None
    if not args.alarm_client:
        parser.error('--tagged-alarm-utilities requires --alarm-client')
    catalog = getattr(args, 'desk_points_face', False)
    pin = CATALOG_COMMIT if catalog else UTILITIES_COMMIT
    headers = (*HEADERS, *POINTS_HEADERS, *CATALOG_HEADERS) if catalog else ((*HEADERS, *POINTS_HEADERS) if getattr(args, 'sparse_start', False) else HEADERS)
    try:
        source = {name: subprocess.check_output(['git', '-C', str(repo), 'show',
            pin + ':' + ('LICENSE' if name == 'LICENSE' else
            'lib/Alarm/include/' + name)], stderr=subprocess.PIPE)
            for name in (*headers, 'LICENSE')}
    except (OSError, subprocess.CalledProcessError) as error:
        parser.error('Cannot read pinned tagged alarm SDK: ' + str(error))
    for name in headers:
        (includes / name).write_bytes(source[name])
    receipt = {'repository': 'michaelrolphone-cmyk/RiscRTE-Utilities',
               'commit': pin, 'capability': 'alarm.service', 'api': 2,
               'sha256': {name: hashlib.sha256(data).hexdigest()
                          for name, data in source.items()}}
    notices = out / 'licenses' / 'tagged-alarm'
    notices.mkdir(parents=True, exist_ok=True)
    (notices / 'LICENSE.txt').write_bytes(source['LICENSE'])
    (notices / 'SOURCES.json').write_text(json.dumps(receipt, indent=2) + '\n')
    (out / 'tagged-alarm-sdk.json').write_text(json.dumps(receipt, indent=2) + '\n')
    return receipt

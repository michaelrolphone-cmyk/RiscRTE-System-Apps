"""Explicit tagged alarm SDK selection for a native-time deployment."""
import hashlib
import json
import subprocess
from pathlib import Path

UTILITIES_COMMIT = 'e80172353fcb9e6519ca9d9b2a4d43c69c1bc91d'
HEADERS = ('AlarmServiceV1.h', 'AlarmServiceV2.h')
CLOCK_EDIT_COMMIT = 'bbafc2c3aebe8c3b9805639f873341e31296c8fb'
CLOCK_EDIT_HEADERS = (*HEADERS, 'AlarmClockEditV2.h')
# Public recovered source: all eight headers and LICENSE are byte-identical
# to historical private9bd57279; see the recovery source lock.
CATALOG_COMMIT = 'e80172353fcb9e6519ca9d9b2a4d43c69c1bc91d'
CATALOG_HEADERS = ('PointsCatalogProjection.h', 'PointsServiceProjection.h')
POINTS_HEADERS = ('AlarmRecords.h', 'PointsRecords.h', 'PointsSchedule.h', 'PointsUtcSchedule.h')

def options(parser):
    parser.add_argument('--clock-edit-authority', action='store_true', help='Explicit tagged native editor-aware alarm service selection')
    parser.add_argument('--tagged-alarm-utilities', type=Path,
                        help='Read the pinned alarm.service@2 SDK from this Utilities Git checkout')

def stage(args, parser, out, includes):
    repo = getattr(args, 'tagged_alarm_utilities', None)
    editor = getattr(args, 'clock_edit_authority', False)
    if repo is None:
        if editor: parser.error('--clock-edit-authority requires --tagged-alarm-utilities')
        return None
    if not args.alarm_client:
        parser.error('--tagged-alarm-utilities requires --alarm-client')
    catalog = getattr(args, 'desk_points_face', False)
    if editor and catalog: parser.error('Clock editor is not a Points profile')
    pin = CLOCK_EDIT_COMMIT if editor else CATALOG_COMMIT if catalog else UTILITIES_COMMIT
    headers = (*HEADERS, *POINTS_HEADERS, *CATALOG_HEADERS) if catalog else ((*HEADERS, *POINTS_HEADERS) if getattr(args, 'sparse_start', False) else HEADERS)
    if editor: headers = CLOCK_EDIT_HEADERS
    if getattr(args, 'contexts_rf_only', False):
        headers = tuple(dict.fromkeys((*headers, 'AlarmRecords.h', 'alarm_writer.h')))
    try:
        source = {name: subprocess.check_output(['git', '-C', str(repo), 'show',
            pin + ':' + ('LICENSE' if name == 'LICENSE' else
            ('Apps/' if name == 'alarm_writer.h' else 'lib/Alarm/include/') + name)], stderr=subprocess.PIPE)
            for name in (*headers, 'LICENSE')}
    except (OSError, subprocess.CalledProcessError) as error:
        parser.error('Cannot read pinned tagged alarm SDK: ' + str(error))
    for name in headers:
        (includes / name).write_bytes(source[name])
    receipt = {'repository': 'michaelrolphone-cmyk/RiscRTE-Utilities',
               'commit': pin, 'capability': 'alarm.service', 'api': 2,
               'sha256': {name: hashlib.sha256(data).hexdigest()
                          for name, data in source.items()}}
    if editor: receipt['clock_edit_authority'] = True
    notices = out / 'licenses' / 'tagged-alarm'
    notices.mkdir(parents=True, exist_ok=True)
    (notices / 'LICENSE.txt').write_bytes(source['LICENSE'])
    (notices / 'SOURCES.json').write_text(json.dumps(receipt, indent=2) + '\n')
    (out / 'tagged-alarm-sdk.json').write_text(json.dumps(receipt, indent=2) + '\n')
    return receipt

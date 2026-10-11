#!/usr/bin/env python3
"""Port the exact frozen Reader catalog to C, never local/current tzdata."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
ROOT = Path(__file__).resolve().parents[1]
PIN = '34d8e694d89a1e72d8854403d8592c289fae3ddc'
SOURCE = 'lib/hal/TimeZoneData.cpp'
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--reader', type=Path, required=True, help='Read-only Reader Git checkout containing the pinned object')
p.add_argument('--check', action='store_true', help='Compare without writing')
a = p.parse_args()
source = subprocess.check_output(['git', '-C', str(a.reader), 'show', PIN + ':' + SOURCE])
provenance = json.loads((ROOT / 'lib/PortableApps/time/TIMEZONE_PROVENANCE.json').read_text())
assert hashlib.sha256(source).hexdigest() == provenance['sources'][SOURCE]['sha256']
rows = re.findall(r'\{"([^"]+)", "([^"]+)", TimeZoneRegion::(\w+)\}', source.decode())
assert len(rows) == 419 and len({row[0] for row in rows}) == 419
assert all(len(zone) < 40 and len(rule) < 96 for zone, rule, region in rows)
output = ('/* Frozen Reader catalog; MIT, Copyright (c) 2025 Dave Allie.\n'
          ' * See ../time/TIMEZONE_PROVENANCE.json and repository LICENSE. */\n'
          '#include "PortableTimeZone.h"\n\nstatic const portable_timezone_entry entries[] = {\n')
output += ''.join(f'    {{"{zone}", "{rule}", PORTABLE_TIMEZONE_{region.upper()}}},\n' for zone, rule, region in rows)
output += ('};\n\nunsigned portable_timezone_count(void) { return (unsigned)(sizeof(entries)/sizeof(entries[0])); }\n'
           'const portable_timezone_entry *portable_timezone_get(unsigned index) {\n'
           '    return index < portable_timezone_count() ? &entries[index] : 0;\n}\n')
target = ROOT / 'lib/PortableApps/src/PortableTimeZoneCatalog.c'
if a.check:
    assert target.read_text() == output, 'Frozen catalog differs'
else:
    target.write_text(output)
print('Exact frozen Reader catalog verified: 419 entries, IDs/rules/order unchanged')

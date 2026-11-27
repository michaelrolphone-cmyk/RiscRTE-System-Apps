#!/usr/bin/env python3
"""Require a reviewed span disposition for every captured replay command.

Unknown numeric/future kinds still execute the production full-height fallback.
This source coverage check is not a substitute for pixel/custody tests.
"""
import json,re
from pathlib import Path
r=Path(__file__).resolve().parents[1]/'lib/PortableApps/src'
s=(r/'raster_snapshot_state.inc').read_text()
kinds=set(re.findall(r'\bRS_\w+',s[s.index('enum {'):s.index('};')]))
s=(r/'raster_snapshot_replay.inc').read_text()
s=s[s.index('static void raster_command_span'):s.index('static bool raster_progress')]
reviewed=set(re.findall(r'case (RS_\w+)',s))
assert kinds==reviewed,{'missing':sorted(kinds-reviewed),'obsolete':sorted(reviewed-kinds)}
print(json.dumps({'declared_command_kinds':len(kinds),'explicitly_reviewed':sorted(reviewed),'unknown_fallback':'full logical height'},indent=2))

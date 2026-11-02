#!/usr/bin/env python3
"""Canonical Home capability declarations, not product hardware authority."""
import json,pathlib
root=pathlib.Path(__file__).resolve().parents[1]
m=json.loads((root/'Apps/shared_home.json').read_text())
required={r['capability']:r['api'] for r in m['requires']}
optional={r['capability']:r['api'] for r in m['optional']}
assert len(required)+len(optional)==28 and len(required)+len(optional)<=32
assert not set(required)&set(optional)
assert required=={'display.output':1,'runtime.realtime-control':1,'runtime.provider-promotion':1,'ui.home-presentation':1,'ui.canvas.raster':1}
assert optional['ui.canvas-input']==1 # Pure retained TIMER needs no input provider.
assert optional['alarm.service']==2 and optional['rtc.clock']==2
assert all(not k.startswith(('x4.','twatch.','waveshare.')) for k in required|optional)
assert not {'input.touch.raw','input.navigation','ui.canvas'} & (required.keys()|optional.keys())
assert {'storage.key-value','storage.preferences','storage.key-value.atomic','storage.key-value.availability','storage.shared-data'}<=optional.keys()
print('Common Home manifest:28 unique declarations, optional foreground input/device/storage features, API2 alarm and no physical input authority PASS')

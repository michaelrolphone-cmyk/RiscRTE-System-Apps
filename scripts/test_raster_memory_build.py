#!/usr/bin/env python3
"""Owning selectors: independent defaults and explicit snapshot/alarm dependencies."""
import argparse,itertools
from types import SimpleNamespace
import portable_raster_memory_build as m
class Reject(Exception):pass
class Parser:
 def error(self,s):raise Reject(s)
count=0
for clear,exchange,sliced,alarm in itertools.product((False,True),repeat=4):
 a=SimpleNamespace(raster_overwrite_clear=clear,raster_underlay_exchange=exchange,sliced_native_frames=sliced,alarm_client=alarm)
 valid=(not(clear or exchange)or sliced)and(not exchange or alarm)
 try:f=m.configure(a,Parser(),['-DPORTABLE_ALARM_CLIENT'] if alarm else []);assert valid
 except Reject:assert not valid;count+=1;continue
 assert ('-DPORTABLE_RASTER_OVERWRITE_CLEAR'in f)==clear
 assert ('-DPORTABLE_RASTER_UNDERLAY_EXCHANGE'in f)==exchange
 assert m.record(a)['overwrite_clear']==clear and m.record(a)['underlay_exchange']==exchange
 count+=1
p=argparse.ArgumentParser();m.add_arguments(p);a=p.parse_args([]);assert m.configure(a,Parser())==[]
assert m.configure(SimpleNamespace(),Parser())==[]
print(f'Raster memory build: {count} combinations plus absent/default argument compatibility PASS')

a=SimpleNamespace(raster_underlay_exchange=True,sliced_native_frames=True,alarm_client=False)
assert m.configure(a,Parser(),['-DPORTABLE_ALARM_CLIENT'])==['-DPORTABLE_RASTER_UNDERLAY_EXCHANGE']
print('Effective foreground alarm flag accepted without explicit argument PASS')

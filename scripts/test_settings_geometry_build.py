#!/usr/bin/env python3
"""Owning Settings replay opt-ins: defaults, independent flags and rejection."""
import contextlib,io,json
import build_portable_settings as b
p=b.argument_parser();base=['--settings-profile','native-time-touch','--native-settings-ui','--sliced-native-frames']
assert b.replay_geometry_selection(p.parse_args([]),p)==([],None)
results=[]
for alpha in [False,True]:
 for prepared in [False,True]:
  args=p.parse_args(base+(['--settings-alpha-geometry'] if alpha else [])+(['--settings-prepared-text'] if prepared else []))
  flags,record=b.replay_geometry_selection(args,p)
  assert ('-DPORTABLE_SETTINGS_ALPHA_GEOMETRY' in flags)==alpha
  assert ('-DPORTABLE_SETTINGS_PREPARED_TEXT' in flags)==prepared
  if record:assert record['alpha_geometry']==alpha and record['prepared_text']==prepared and not record['physical_acceptance']
  results.append(dict(alpha=alpha,prepared=prepared,flags=flags,record=record))
for option in ['--settings-alpha-geometry','--settings-prepared-text']:
 for extra in [[],['--settings-profile','native-time-touch'],base+['--paper-transitions'],['--settings-profile','x4-native-time','--native-settings-ui','--sliced-native-frames']]:
  with contextlib.redirect_stderr(io.StringIO()):
   try:b.replay_geometry_selection(p.parse_args(extra+[option]),p)
   except SystemExit as error:assert error.code==2
   else:raise AssertionError('Invalid selection accepted')
print(json.dumps(dict(default_unchanged=True,independent_combinations=results,rejected_combinations=8),indent=2))

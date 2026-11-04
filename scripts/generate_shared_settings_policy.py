#!/usr/bin/env python3
"""Generate explicit deployment semantics; never infer policy from hardware."""
import argparse,json,pathlib
p=argparse.ArgumentParser();p.add_argument('policy',type=pathlib.Path);p.add_argument('output',type=pathlib.Path);a=p.parse_args();d=json.loads(a.policy.read_text())
fields=['features','writable','sleep_modes','sleep_default','face_default','idle_default_ms','deep_default_ms','desk_lock','visual_alerts','clock_mode','storage_mode','rtc_instance']
if set(d)!=set(fields):p.error('Exactly these semantic fields are required: '+', '.join(fields))
for k,v in d.items():
 if k in ('desk_lock','visual_alerts'):
  if not isinstance(v,bool):p.error(k+' must be boolean')
 elif not isinstance(v,int) or isinstance(v,bool) or v<0:p.error(k+' must be a nonnegative integer')
if d['features']&~16383 or d['writable']&~d['features']:p.error('invalid feature/writable masks')
if d['sleep_modes']&~7 or d['sleep_default']>2 or not d['sleep_modes']&(1<<d['sleep_default']):p.error('invalid explicit sleep policy')
if d['clock_mode']>2 or bool(d['features']&1)!=(d['clock_mode']!=0):p.error('TIME feature and clock mode disagree')
if d['clock_mode']!=2 and d['rtc_instance']:p.error('RTC instance requires RTC clock mode')
if d['storage_mode']>2:p.error('invalid storage mode')
if d['face_default']>=7:p.error('invalid face default')
if d['rtc_instance']>=1<<64:p.error('RTC instance exceeds uint64')
for key,minimum in [('idle_default_ms',5000),('deep_default_ms',60000)]:
 if not minimum<=d[key]<=3600000 or d[key]%1000:p.error('invalid '+key)
# Apply hooks cannot be invented. This provider declares persistence-only changes;
# deployment owners must qualify host reload behavior before selecting it.
model=', '.join('.'+k+'='+str(int(d[k]))+'u' for k in fields[:9])
text='#pragma once\n#define SETTINGS_POLICY_INITIALIZER { .api_version=1, .struct_size=sizeof(portable_settings_policy_v1), .model={'+model+'}, .clock_mode='+str(d['clock_mode'])+'u, .storage_mode='+str(d['storage_mode'])+'u, .rtc_instance=UINT64_C('+str(d['rtc_instance'])+'), .apply_features=0 }\n'
a.output.mkdir(parents=True,exist_ok=True);(a.output/'settings_policy_config.h').write_text(text);(a.output/'selected-policy.json').write_text(json.dumps(d,indent=2)+'\n')

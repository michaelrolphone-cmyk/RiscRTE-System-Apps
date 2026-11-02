#!/usr/bin/env python3
"""Generate the reviewed product overlay from the exact delivered cohort.
Product wiring lives here, not in the shared application. Existing grants,
namespace keys, aliases and SD routes are retained verbatim.
"""
import argparse,copy,json,hashlib
from pathlib import Path
from assemble_shared_home_cohort import read_lane,inventory,fingerprint
from compose_shared_app_cohort import digest,encode
ROOT=Path(__file__).resolve().parents[1]
COMMON_GRANTS=[('ui.home-presentation',1),('ui.canvas.raster',1),('ui.canvas-input',1),('alarm.service.facade',2),('time.civil',1)]
SCENES={'watch':'watch-compact','wave':'color-dynamic','x4':'full'}
SCENE_OLD={'watch':'89a38aba58957f18e9dc82588856589e20309368701a2158a96cfc7f0e041990','wave':'83d3dec48b61e0d61dfccd06982ad2d027432f2f80b16491c928dbcbae822867','x4':'8f841c721085de220e16460e07b2751be3673487838ab4c6915bf7d5405d3122'}
def canonical_hash(value):
 return hashlib.sha256((json.dumps(value,sort_keys=True,separators=(',',':'))+'\n').encode()).hexdigest()
def grant(cap,api=1,instance=0):return {'capability':cap,'api':api,'instance_id':instance}
def row_for(boot,path):
 rows=[r for r in boot['drivers'] if r['manifest']==path]
 if len(rows)!=1:raise ValueError('Selected driver row not unique '+path)
 return copy.deepcopy(rows[0])
def package(packages,name,row):
 root=packages/name
 return {'package':name,'manifest_sha256':digest((root/'manifest.json').read_bytes()),
         'elf_sha256':digest((root/'driver.elf').read_bytes()),'row':row}
def replacement(packages,name,path,entries,sd,boot):
 item=package(packages,name,row_for(boot,path));binary=str(Path(path).parent/'driver.elf')
 item['replace']={'manifest_sha256':digest(entries[path]),'elf_sha256':digest(entries[binary])}
 source=item['row'].get('image_source')
 if source:
  if source.get('internal_fallback') is not True:raise ValueError('Unreviewed external provider route')
  item['replace']['external_route']={'path':source['path'],'elf_sha256':digest(sd[source['path']])}
 return item
def generate(cohort,packages,baseline):
 plans={}
 for lane in ('watch','wave','x4'):
  entries,sd=read_lane(cohort,lane);actual=inventory(entries,sd);boot=actual['boot'];expected=baseline['products'][lane]
  if canonical_hash(boot)!=expected['boot_sha256']:raise ValueError('Reviewed authority changed '+lane)
  for path,manifest_hash in expected['manifest_sha256'].items():
   if canonical_hash(actual['manifests'].get(path))!=manifest_hash:raise ValueError('Reviewed installed contract changed '+lane+'/'+path)
  if digest(entries['ui-scene/driver.elf'])!=SCENE_OLD[lane]:raise ValueError('Original selected scene profile differs '+lane)
  policy=[r for r in boot['app_capabilities'] if r['manifest']=='default.json']
  if len(policy)!=1:raise ValueError('Home policy not unique')
  updated=copy.deepcopy(policy)
  for cap,api in COMMON_GRANTS:
   g=grant(cap,api)
   if g not in updated[0]['grants']:updated[0]['grants'].append(g)
  if lane=='x4':
   for cap in ('power.sleep','display.power'):updated[0]['grants'].append(grant(cap))
  if len(updated[0]['grants'])>32:raise ValueError('Home declared grant capacity exceeds qualified profile')
  viewer=row_for(boot,'viewer-canvas/manifest.json')
  physical={b['capability']:b for b in viewer['bindings']}
  raster={'manifest':'home-raster/manifest.json','bindings':[copy.deepcopy(physical[c]) for c in ('display.output','ui.presentation-profile')]}
  inputs={'manifest':'home-input/manifest.json','bindings':[grant('ui.canvas.raster')]+[copy.deepcopy(physical[c]) for c in ('input.touch.raw','input.navigation') if c in physical]}
  selected=[
   package(packages,'raster',raster),
   package(packages,'input' if lane=='x4' else 'input-touch',inputs),
   package(packages,'presentation-overview' if lane=='x4' else 'presentation-faces',{'manifest':'home-presentation/manifest.json'}),
   package(packages,'alarm-tagged' if lane=='x4' else 'alarm-legacy',{'manifest':'home-alarm/manifest.json','bindings':[grant('alarm.service',2 if lane=='x4' else 1)]}),
   replacement(packages,'civil-x4' if lane=='x4' else 'civil-watch','civil-clock-x4/manifest.json' if lane=='x4' else 'civil-clock/manifest.json',entries,sd,boot),
   replacement(packages,'scene-'+SCENES[lane],'ui-scene/manifest.json',entries,sd,boot)]
  if lane=='x4':
   selected += [
    package(packages,'x4-display-power',{'manifest':'home-display-power/manifest.json','bindings':[grant('display.output',1,3),grant('storage.volume',1,9),grant('display.frontlight',1,5),grant('x4.power',1,17)]}),
    package(packages,'x4-power',{'manifest':'home-power/manifest.json','bindings':[grant('x4.power',1,17),grant('display.power')]}),
    replacement(packages,'x4-physical-power','power/manifest.json',entries,sd,boot)]
  rows=[r for r in boot['app_capabilities'] if r['manifest']=='contexts.json']
  if len(rows)!=1:raise ValueError('Contexts policy not unique')
  changed=copy.deepcopy(rows)
  if lane=='x4':changed[0]['grants'].append(grant('app.catalog'))
  source=rows[0].get('image_source')
  old_manifest=json.loads(entries['contexts.json'])
  route={'storage':'sd' if source else 'internal','path':source['path'] if source else old_manifest['file_name']}
  oldelf=(sd if source else entries)[route['path']]
  ctx=packages/'contexts'
  update={'manifest':'contexts.json','package':'contexts','manifest_file':'contexts.json','elf_file':'contexts.elf',
          'manifest_sha256':digest((ctx/'contexts.json').read_bytes()),'elf_sha256':digest((ctx/'contexts.elf').read_bytes()),
          'route':route,'original_policy_rows':rows,'policy_rows':changed,
          'replace':{'manifest_sha256':digest(entries['contexts.json']),'elf_sha256':digest(oldelf)}}
  plans[lane]={'inventory_sha256':fingerprint(actual),'aliases':[{'role':'controller','manifest':'default.json','route':{'storage':'internal','path':'default.elf'},'original_policy_rows':policy,'policy_rows':updated}],
               'providers':selected,'app_updates':[update]}
 return plans
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('cohort','packages','output'):p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--baseline',type=Path,default=ROOT/'docs/shared-home/cohort-authority-baseline.json')
 a=p.parse_args()
 if a.output.exists():raise ValueError('Fresh reviewed plan output required')
 a.output.write_bytes(encode(generate(a.cohort,a.packages,json.loads(a.baseline.read_text()))))

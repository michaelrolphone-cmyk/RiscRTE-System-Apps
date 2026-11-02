#!/usr/bin/env python3
"""Generate the reviewed product overlay from the exact delivered cohort.
Product wiring lives here, not in the shared application. Existing grants,
namespace keys, aliases and SD routes are retained verbatim.
"""
import argparse,copy,json,hashlib
from pathlib import Path
from assemble_shared_home_cohort import read_lane,inventory,fingerprint,checked_native
from compose_shared_app_cohort import digest,encode
ROOT=Path(__file__).resolve().parents[1]
COMMON_GRANTS=[('ui.home-presentation',1),('ui.canvas.raster',1),('ui.canvas-input',1),('alarm.service.facade',2),('time.civil',1)]
SCENES={'watch':'watch-compact','wave':'color-dynamic','x4':'full'}
SCENE_OLD={'watch':'89a38aba58957f18e9dc82588856589e20309368701a2158a96cfc7f0e041990','wave':'83d3dec48b61e0d61dfccd06982ad2d027432f2f80b16491c928dbcbae822867','x4':'8f841c721085de220e16460e07b2751be3673487838ab4c6915bf7d5405d3122'}
# Exact existing deployment metadata observed in owning hosted run41453730076,
# job125305053162 at a67a1ab8; source keys are preserved, never invented.
NATIVE_COHORT_BASELINE={
 "watch": {
  "firmware_sha256": "4a3c866f574af8fafa5f794e37d22c45c74a1afb4e2d37fc0e51041628e80bb4",
  "firmware_size": 1631440,
  "layout": "riscrte-paired-appdata-v3",
  "product": "twatch-s3",
  "runtime_version": "0.4.18",
  "schema": "riscrte.cohort",
  "schema_version": 1,
  "source_repo": "michaelrolphone-cmyk/RiscRTE-T-Watch-S3",
  "source_revision": "2050c9f4c40b4951efdc9a59edd9d2c871c30ed7",
  "store_abi": 3,
  "version": "1.1.47"
 },
 "wave": {
  "firmware_sha256": "3710e874fe75dbb30054ec2507563e45ae5bd8a4a892e0d90d23b681c6eca4f4",
  "firmware_size": 1816368,
  "layout": "riscrte-paired-appdata-v3",
  "product": "waveshare-s3-touch-amoled-206",
  "runtime_version": "0.4.18",
  "schema": "riscrte.cohort",
  "schema_version": 1,
  "source_repo": "michaelrolphone-cmyk/RiscRTE-Wavshare-S3-Touch-AMOLED-2.06",
  "source_revision": "a6a85f7100aae7221e116a0e00200ea982338390",
  "store_abi": 3,
  "version": "0.1.65"
 },
 "x4": {
  "firmware_sha256": "cf8e376ebf1f92142d2ff8bdf8bbabfea2579cd8598548202b64728065da4a13",
  "firmware_size": 1804976,
  "layout": "riscrte-paired-appdata-v2",
  "product": "xteink-x4-pro",
  "runtime_version": "0.4.7",
  "schema": "riscrte.cohort",
  "schema_version": 1,
  "source_repo": "michaelrolphone-cmyk/RiscRTE-XTEINK-X4-PRO",
  "source_revision": "93ee386b7e23628f3776edc6cfd2805cf675d649",
  "store_abi": 2,
  "version": "0.1.123"
 }
}
CANDIDATE_PRODUCT_VERSIONS={'watch':'1.1.48','wave':'0.1.66','x4':'0.1.124'}
def native_metadata_plan(entries,packages,lane):
 old=json.loads(entries['cohort.json'])
 if old!=NATIVE_COHORT_BASELINE[lane]:raise ValueError('Reviewed native cohort metadata changed '+lane)
 package_name='native-'+lane
 descriptor_sha256=digest((packages/package_name/'native.json').read_bytes())
 native=checked_native(packages,package_name,descriptor_sha256)
 if native['artifact']['repository']!=old['source_repo']:raise ValueError('Native source repository differs '+lane)
 return {'path':'cohort.json','before_sha256':digest(entries['cohort.json']),
         'package':package_name,'descriptor_sha256':descriptor_sha256,
         'product_version':CANDIDATE_PRODUCT_VERSIONS[lane],
         'fields':{key:{'path':[key],'before':old[key]} for key in
                   ('firmware_size','firmware_sha256','runtime_version','version','source_revision')}}

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
               'providers':selected,'app_updates':[update],'native_metadata':native_metadata_plan(entries,packages,lane)}
 return plans
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('cohort','packages','output'):p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--baseline',type=Path,default=ROOT/'docs/shared-home/cohort-authority-baseline.json')
 a=p.parse_args()
 if a.output.exists():raise ValueError('Fresh reviewed plan output required')
 a.output.write_bytes(encode(generate(a.cohort,a.packages,json.loads(a.baseline.read_text()))))

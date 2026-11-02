#!/usr/bin/env python3
"""Build dependency-free Home semantic presentation policies, never app variants."""
import argparse,json,pathlib,tempfile,shutil
from scene_sdk import stage_sdk
from scene_build import build,compiler_path,json_write
ROOT=pathlib.Path(__file__).resolve().parents[1]
def run(runtime,output):
 output.mkdir(parents=True,exist_ok=False);rows=[]
 with tempfile.TemporaryDirectory(prefix='home-presentation-') as tmp:
  include=stage_sdk(runtime,ROOT,pathlib.Path(tmp))
  shutil.copyfile(ROOT/'sdk/app/RiscHomePresentationV1.h',include/'RiscHomePresentationV1.h')
  compiler=compiler_path()
  for name,view,namespace in [('faces',1,61),('overview',2,62)]:
   manifest=json.loads((ROOT/'Services/home_presentation'/f'{name}.json').read_text())
   flags=[f'-DHOME_POLICY_VIEW={view}',f'-DHOME_POLICY_CRASH_NAMESPACE={namespace}',f'-DHOME_POLICY_ID="home-presentation-{name}"']
   row=build(compiler,include,output/name,manifest,[ROOT/'Services/home_presentation/policy.c'],flags)
   repeat=build(compiler,include,output/(name+'-repeat'),manifest,[ROOT/'Services/home_presentation/policy.c'],flags)
   if row['sha256']!=repeat['sha256']:raise ValueError('Policy repeated ELF differs')
   rows.append(dict(row,semantic_default=view,crash_namespace=namespace,catalog_volume='unique permitted instance'))
  json_write(output/'packages.json',{'schema':1,'policies':rows,'application_variants':False,'hardware_testing':'not performed'})
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--runtime',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args();run(a.runtime.resolve(),a.output.resolve())

#!/usr/bin/env python3
"""Full-feature shared UI target proof with immutable generic bridge inputs."""
import argparse,hashlib,json,os,re,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--compiler',required=True);p.add_argument('--exact-inputs',required=True,type=Path)
p.add_argument('--bridges',required=True,type=Path);p.add_argument('--utilities',required=True,type=Path);a=p.parse_args()
BRIDGE='6e82fdcaec0b73ed700d2104cfb4d94f011c19d4'
PINS={'settings':'80efb298c9ca8d0f62f9a31840c9e51d380b40f6','springboard':'fc0bdbbbc600ce891e60c44d091fce0586ade7d8'}
OUT=ROOT/'build/runtime-app-catalog/generic-target';OUT.mkdir(parents=True,exist_ok=True)
cc=str(Path(a.compiler).resolve());nm=cc.removesuffix('gcc')+'nm';reports=[]
def materialize(repo,pin,paths,destination):
 names=subprocess.check_output(['git','ls-tree','-r','--name-only',pin,*paths],cwd=repo,text=True).splitlines()
 for name in names:
  path=Path(name)
  if path.suffix not in ('.h','.inc') and path.name not in ('portable_navigation.c','portable_sleep.c'):continue
  out=destination/path;out.parent.mkdir(parents=True,exist_ok=True)
  out.write_bytes(subprocess.check_output(['git','show',pin+':'+name],cwd=repo))
with tempfile.TemporaryDirectory(prefix='shared-catalog-target-') as temporary:
 temp=Path(temporary);bridge=temp/'bridges';materialize(a.bridges,BRIDGE,['apps/clock','sdk','include'],bridge)
 for app,pin in PINS.items():
  utility=temp/('utilities-'+app);materialize(a.utilities,pin,['Apps','lib/Alarm/include','lib/Contexts/include'],utility)
  recipe=json.loads((a.exact_inputs/(app+'-exact-build.json')).read_text())['command']
  system_base=next(x.split('/lib/PortableApps/',1)[0] for x in recipe if x.endswith('/lib/PortableApps/src/adapter.c'))
  hashes=[];compiled=[]
  for inventory in ('first','second'):
   out=temp/(app+'-'+inventory);out.mkdir();mapping=out/(app+'.map');mapping.write_text('{ global: app_main; app_module_init; app_module_fini; local: *; };\n')
   (out/'inventory.json').write_text(json.dumps({'installed':[inventory+'.elf'],'available':inventory=='first'}))
   (out/'catalog.c').write_text('#error Compiled inventory must never be linked\n')
   command=[cc];skip=False
   for i,arg in enumerate(recipe[1:],1):
    if skip:skip=False;continue
    if arg=='-o':skip=True;continue
    if arg=='-include':
     skip=True;source=Path(recipe[i+1]);candidate=a.exact_inputs/source.name
     if source.name=='ContextsFaces.h':candidate=bridge/'apps/clock/faces/ContextsFaces.h'
     assert candidate.is_file(),candidate
     command+=['-include',str(candidate)];continue
    if arg.startswith('-Wl,--version-script='):command.append('-Wl,--version-script='+str(mapping));continue
    if arg.endswith(('/catalog.c','/empty_catalog.c','/springboard-catalog.c')):continue
    if arg.startswith('-I'):
     value=arg[2:]
     if value.startswith(system_base+'/'):value=str(ROOT/value[len(system_base)+1:])
     elif '/utilities/' in value:value=str(utility/value.split('/utilities/',1)[1])
     elif '/watch-profile/' in value:value=str(bridge/value.split('/watch-profile/',1)[1])
     elif '/system-files-logs/' in value:value=str(ROOT/value.split('/system-files-logs/',1)[1])
     assert Path(value).is_dir(),value
     command.append('-I'+value);continue
    if arg.endswith(('/portable_navigation.c','/portable_sleep.c')):arg=str(bridge/'apps/clock'/Path(arg).name)
    elif arg.startswith(system_base+'/'):arg=str(ROOT/arg[len(system_base)+1:])
    if arg.endswith(('.c','.cpp','.cc')):assert Path(arg).is_file(),arg
    command.append(arg)
   # New shared custody hook and native geometry are additive; original audited
   # feature switches remain unchanged, including their distinct app versions.
   command+=['-DPORTABLE_WATCH_NATIVE_HEIGHT','-DPORTABLE_NAVIGATION_RETENTION','-o',str(out/(app+'.elf'))]
   subprocess.run(command,check=True,timeout=180)
   binary=out/(app+'.elf');data=binary.read_bytes();hashes.append(hashlib.sha256(data).hexdigest())
   assert b'Compiled inventory' not in data
   all_symbols=subprocess.check_output([nm,str(binary)],text=True)
   assert not any(line.split()[-1] in ('portable_catalog','portable_catalog_count') for line in all_symbols.splitlines() if line.split())
   dynamic=subprocess.check_output([nm,'-D',str(binary)],text=True)
   imports=sorted({line.split()[-1] for line in dynamic.splitlines() if ' U ' in ' '+line})
   assert set(imports)<={'risc_runtime_get_api','memcpy','memset','memcmp','memchr','strcmp','strncmp','strlen','snprintf','strcpy','malloc','free'},imports
   exports=sorted({line.split()[-1] for line in dynamic.splitlines() if len(line.split())>=3 and line.split()[-2] in ('T','D','B','R')})
   assert exports==['app_main','app_module_fini','app_module_init'],exports
   # Ask the same preprocessor for every actual transitive input; unused include
   # search directories cannot masquerade as a dependency/provenance claim.
   dependencies=subprocess.check_output([*command[:-2],'-M'],text=True,timeout=120)
   paths=sorted({Path(t).resolve() for t in shlex.split(dependencies.replace('\\\n',' ')) if not t.endswith(':') and Path(t).is_file()})
   sources={}
   for path in paths:
    key=str(path)
    for directory,prefix in ((ROOT,'System'),(bridge,'SharedWatch@'+BRIDGE),(utility,'Utilities@'+pin),(a.exact_inputs.resolve(),'AuditedProfile')):
     if path.is_relative_to(directory):key=prefix+'/'+str(path.relative_to(directory));break
    sources[key]=hashlib.sha256(path.read_bytes()).hexdigest()
   compiled.append({'inventory':inventory,'sha256':hashes[-1],'size_bytes':len(data),'imports':imports,'exports':exports,
                    'command':command,'source_sha256':sources,'dependency_output_sha256':hashlib.sha256(dependencies.encode()).hexdigest()})
   if inventory=='first':(OUT/(app+'.elf')).write_bytes(data)
  assert hashes[0]==hashes[1],(app,hashes)
  assert compiled[0]['source_sha256']==compiled[1]['source_sha256']
  reports.append({'app':app,'sha256':hashes[0],'inventory_independent':True,'utilities_revision':pin,'builds':compiled})
report={'system_revision':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
 'system_working_tree_dirty':bool(subprocess.check_output(['git','diff','--name-only','HEAD','Apps','lib'],cwd=ROOT,text=True).strip()),
 'shared_bridges_revision':BRIDGE,'compiler':subprocess.check_output([cc,'--version'],text=True).splitlines()[0],
 'results':reports,'hardware_qualified':False,'runtime_capability':'app.catalog@1'}
(OUT/'receipt.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'receipt':str(OUT/'receipt.json'),'apps':[{k:r[k] for k in ('app','sha256','utilities_revision')} for r in reports]},indent=2))

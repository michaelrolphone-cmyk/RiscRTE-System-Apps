#!/usr/bin/env python3
"""Target ELF independence from old generated product inventory sources.

The optional exact recipes preserve all original feature flags and local bridge
sources. Standard builders are checked independently; no firmware is built.
"""
import argparse,hashlib,json,os,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--compiler',required=True);p.add_argument('--exact-inputs',type=Path)
a=p.parse_args();cc=str(Path(a.compiler).resolve());env={**os.environ,'NATIVE_APP_CC':cc}
reports=[]
with tempfile.TemporaryDirectory(prefix='catalog-target-build-') as temporary:
 temp=Path(temporary)
 for app in ('settings','springboard'):
  hashes=[]
  for inventory in ('first','second'):
   out=temp/(app+'-'+inventory);out.mkdir()
   # Poisoned legacy files are valid catalog inputs, but must never be consumed.
   for name in ('catalog.c','springboard-catalog.c','empty_catalog.c'):
    (out/name).write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[]={{.display_name="INVENTORY_'+inventory+'",.file_name="'+inventory+'.elf",.compatible=true}};\nconst unsigned portable_catalog_count=1;\n')
   args=['--nova-ui','--output-dir',str(out)]
   if app=='springboard':args+=['--catalog-limit','17' if inventory=='first' else '23']
   subprocess.run([os.sys.executable,str(ROOT/f'scripts/build_portable_{app}.py'),*args],env=env,check=True)
   data=(out/(app+'.elf')).read_bytes()
   assert b'INVENTORY_' not in data
   symbols=subprocess.check_output([cc.removesuffix('gcc')+'nm',str(out/(app+'.elf'))],text=True)
   assert not any(line.split()[-1] in ('portable_catalog','portable_catalog_count') for line in symbols.splitlines() if line.split())
   hashes.append(hashlib.sha256(data).hexdigest())
   assert {'capability':'app.catalog','api':1} in json.loads((out/(app+'.json')).read_text())['requires']
  assert hashes[0]==hashes[1],(app,hashes)
  reports.append({'app':app,'profile':'standard-builder','sha256':hashes[0],'inventory_independent':True})
 if a.exact_inputs:
  for app in ('settings','springboard'):
   recipe=json.loads((a.exact_inputs/(app+'-exact-build.json')).read_text())['command'];hashes=[]
   for inventory in ('first','second'):
    out=temp/('exact-'+app+'-'+inventory);out.mkdir();mapping=out/(app+'.map')
    mapping.write_text('{ global: app_main; app_module_init; app_module_fini; local: *; };\n')
    command=[cc];skip=False
    for i,arg in enumerate(recipe[1:],1):
     if skip:skip=False;continue
     if arg=='-o':skip=True;continue
     if arg=='-include':
      source=Path(recipe[i+1]);skip=True
      candidate=a.exact_inputs/source.name
      if not candidate.is_file():candidate=ROOT/'test/native_apps/fixtures/watch_profile'/source.name
      assert candidate.is_file(),candidate
      command+=['-include',str(candidate)];continue
     if arg.startswith('-Wl,--version-script='):command.append('-Wl,--version-script='+str(mapping));continue
     if arg.endswith(('/catalog.c','/empty_catalog.c','/springboard-catalog.c')):continue
     if arg.startswith('-I'):
      suffix=arg[2:]
      # Rebase shared System includes only; local bridge dependencies remain
      # the exact archived sources explicitly selected by the recipe.
      for marker in ('/lib/PortableApps/','/lib/NativeApps/'):
       if marker in suffix:suffix=str(ROOT/('lib/'+suffix.split('/lib/',1)[1]));break
      assert Path(suffix).is_dir(),suffix
      command.append('-I'+suffix);continue
     if arg.endswith(('.c','.cpp','.cc')):
      if '/lib/PortableApps/' in arg or '/lib/NativeApps/' in arg:arg=str(ROOT/('lib/'+arg.split('/lib/',1)[1]))
      elif arg.endswith('/Apps/'+app+'.c'):arg=str(ROOT/'Apps'/(app+'.c'))
      assert Path(arg).is_file(),arg
     command.append(arg)
    command+=['-DPORTABLE_WATCH_NATIVE_HEIGHT','-o',str(out/(app+'.elf'))]
    subprocess.run(command,check=True,timeout=180)
    data=(out/(app+'.elf')).read_bytes();hashes.append(hashlib.sha256(data).hexdigest())
    symbols=subprocess.check_output([cc.removesuffix('gcc')+'nm',str(out/(app+'.elf'))],text=True)
    assert not any(line.split()[-1] in ('portable_catalog','portable_catalog_count') for line in symbols.splitlines() if line.split())
   assert hashes[0]==hashes[1],(app,hashes)
   reports.append({'app':app,'profile':'exact-original-features','sha256':hashes[0],'inventory_independent':True,
                   'defines':[x for x in command if x.startswith('-D')],
                   'hardware_qualification':False,'bridge_sources':'Preserved exact recipe inputs; product composer must qualify generic bridges separately'})
report={'target':'Xtensa ELF applications only','results':reports,'hardware_qualified':False}
out=ROOT/'build/runtime-app-catalog';out.mkdir(parents=True,exist_ok=True)
(out/'target-build-independence.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))

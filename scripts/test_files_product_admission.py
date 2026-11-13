#!/usr/bin/env python3
"""Prepare exact private products with published native binding fixtures.
This verifies the real Runtime graph and target ELF role/import admission,
including retained X4 SD graph. It does not execute target instructions or hardware.
"""
import argparse,pathlib,json,subprocess,hashlib,shlex,os
ROOT=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for name in ('runtime','x4-runtime','assembly','output'):p.add_argument('--'+name,type=pathlib.Path,required=True)
p.add_argument('--prepare-only',action='store_true');p.add_argument('--lane',choices=['watch','wave','x4']);p.add_argument('--source-pins',type=pathlib.Path);p.add_argument('--sanitize',action='store_true');args=p.parse_args();args.assembly=args.assembly.resolve();args.output.mkdir(parents=True,exist_ok=False)
fixtures=ROOT/'test/files_products/native';templates=json.loads((fixtures/'commands.json').read_text());results={}
sha=lambda p:hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
for lane,template in templates.items():
 if args.lane and lane!=args.lane:continue
 cmd=[x.format(runtime=args.runtime,x4_runtime=args.x4_runtime,fixtures=fixtures,output=args.output) for x in template]
 cmd[1:1]=['-DSTORE_ADMISSION_COHORT','-Wno-misleading-indentation']
 cmd+=['-I'+str((args.x4_runtime if lane=='x4' else args.runtime)/'lib/elf_loader/include'),'-I'+str((args.x4_runtime if lane=='x4' else args.runtime)/'test/native_bank_stubs')]
 if args.sanitize:cmd[1:1]=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 selected_runtime=args.x4_runtime if lane=='x4' else args.runtime
 validator=args.output/(lane+'-validate.o');vsrc=selected_runtime/'lib/elf_loader/src/esp_elf_validate.c'
 vcmd=['cc','-std=c11',*(['-fsanitize=address,undefined','-fno-sanitize-recover=all'] if args.sanitize else []),'-I'+str(selected_runtime/'lib/elf_loader/include'),'-I'+str(selected_runtime/'test/native_bank_stubs'),'-c',str(vsrc),'-o',str(validator)]
 subprocess.run(vcmd,check=True);cmd.append(str(validator))
 vdeps=subprocess.check_output(vcmd[:vcmd.index('-c')]+['-MM',str(vsrc)],text=True);validator_dependencies=shlex.split(vdeps.replace('\\\n',' ').split(':',1)[1])
 sources=[x for x in cmd if x.endswith(('.c','.cpp'))];base=[];skip=False
 for x in cmd[1:]:
  if skip:skip=False;continue
  if x=='-o':skip=True;continue
  if x in sources or x in ['-ldl','-rdynamic'] or x.endswith('.o'):continue
  base.append(x)
 files=set(validator_dependencies);per={str(vsrc):validator_dependencies}
 for src in sources:
  q=subprocess.run([cmd[0],*base,'-MM','-MT','source',src],capture_output=True,text=True,check=True);deps=shlex.split(q.stdout.replace('\\\n',' ').split(':',1)[1]);per[src]=deps;files.update(deps)
 before={f:sha(f) for f in sorted(files)}
 before[str(vsrc)]=sha(vsrc)
 portable={}
 for f,h in before.items():
  path=pathlib.Path(f).resolve()
  for role,root in [('fixtures',fixtures),('runtime',selected_runtime)]:
   try:portable[role+'/'+str(path.relative_to(root.resolve()))]=h;break
   except ValueError:pass
  else:raise ValueError('Unmapped compiler dependency: '+f)
 if args.source_pins:assert portable==json.loads(args.source_pins.read_text())[lane],lane+' source pin mismatch'
 (args.output/(lane+'-source-pins.json')).write_text(json.dumps(portable,indent=2)+'\n')
 q=subprocess.run(cmd,capture_output=True);(args.output/(lane+'-compile.log')).write_bytes(q.stdout+q.stderr)
 if q.returncode:raise RuntimeError(lane+' compile failed; see retained log')
 rows=[]
 for which in (() if args.prepare_only else ('after',)):
  q=subprocess.run([str(args.output/lane),str(args.assembly/lane/'before'),str(args.assembly/lane/which)],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
  (args.output/(lane+'-'+which+'.log')).write_text(q.stdout+q.stderr)
  if q.returncode:raise RuntimeError(lane+' '+which+' preparation failed')
  result=json.loads(q.stdout)
  print(json.dumps({'lane':lane,'input':which,'admission':result}),flush=True)
  if not result['prepared'] or not result['cohort_validated'] or not result['elf_count'] or result['hardware_calls'] or result['storage_calls']:raise RuntimeError(lane+' invalid preparation')
  if lane=='x4' and not result.get('retained_sd_serial_graph_bound'):raise RuntimeError('X4 retained SD graph missing')
  rows.append({'input':which,'result':result})
 if {f:sha(f) for f in before}!=before:raise RuntimeError('Compiler input mutated')
 results[lane]={'command':cmd,'inputs':before,'per_tu_dependencies':per,'results':rows,'binary_sha256':sha(args.output/lane)}
 (args.output/'receipt.json').write_text(json.dumps({'schema':1,'sanitized':args.sanitize,'scope':__doc__,'products':results},indent=2)+'\n');print(lane,'PASS',flush=True)

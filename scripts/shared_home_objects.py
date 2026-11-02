"""Compile a complete semantic view into a namespaced relocatable object.
This is one application, not embedded secondary ELFs. Existing view recipes
supply their exact sources/feature flags; no device-target profile is accepted.
"""
import hashlib,json,pathlib,subprocess
def emit_view(compiler,output,prefix,flags,includes,sources):
 output=pathlib.Path(output);output.mkdir(parents=True,exist_ok=False)
 if prefix not in ('home_faces_','home_overview_'):raise ValueError('semantic view prefix required')
 compiler=str(compiler);tool=compiler.removesuffix('gcc')
 # Explicit common no-LTO code generation; never mix compiler/LTO archives.
 compile_flags=[f for f in flags if f!='-flto' and not f.startswith('-Wl,')]
 objects=[];commands=[]
 for index,source in enumerate(sources):
  source=pathlib.Path(source);obj=output/(str(index)+'.o')
  if source.suffix=='.o':
   # Prebuilt objects are accepted only from the same owning invocation recipe.
   obj.write_bytes(source.read_bytes())
  else:
   language='-std=c++11' if source.suffix in ('.cpp','.cc') else '-std=c11'
   cc=tool+'g++' if source.suffix in ('.cpp','.cc') else compiler
   command=[cc,language,'-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-ffunction-sections','-fdata-sections','-Wall','-Wextra','-Werror',*compile_flags,*['-I'+str(p) for p in includes],'-c',str(source),'-o',str(obj)]
   subprocess.run(command,check=True);commands.append(command)
  objects.append(obj)
 aggregate=output/'view-unprefixed.o'
 command=[compiler,'-nostdlib','-nostartfiles','-r',*map(str,objects),'-o',str(aggregate)]
 subprocess.run(command,check=True);commands.append(command)
 symbols=subprocess.check_output([tool+'nm','--defined-only','--extern-only',str(aggregate)],text=True)
 names=sorted({line.split()[-1] for line in symbols.splitlines() if len(line.split())>=3})
 if 'app_main' not in names:raise ValueError('view lacks application controller')
 mapping=output/'symbols.map';mapping.write_text(''.join(name+' '+prefix+name+'\n' for name in names))
 target=output/'view.o';subprocess.run([tool+'objcopy','--redefine-syms='+str(mapping),str(aggregate),str(target)],check=True)
 unresolved=subprocess.check_output([tool+'nm','--undefined-only',str(target)],text=True)
 receipt={'prefix':prefix,'commands':commands,'defined_symbols':names,'undefined_symbols':unresolved.splitlines(),'object_sha256':hashlib.sha256(target.read_bytes()).hexdigest(),'sources':{str(p):hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest() for p in sources},'compiler':subprocess.check_output([compiler,'--version'],text=True)}
 (output/'view.json').write_text(json.dumps(receipt,indent=2)+'\n');return target

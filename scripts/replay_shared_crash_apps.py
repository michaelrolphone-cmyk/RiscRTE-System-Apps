#!/usr/bin/env python3
"""Rebuild qualified crash-repaired Watch ELFs from public, pinned source inputs.

Requires Xtensa ESP32-S3 GCC 8.4.0 and pyelftools.
"""
import argparse,hashlib,importlib.util,io,json,shutil,subprocess,sys,tarfile
from pathlib import Path
from elftools.elf.elffile import ELFFile
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--product',choices=['wave','twatch','wave-media'],required=True)
p.add_argument('--compiler',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--spec-dir',type=Path,default=Path(__file__).resolve().parents[1]/'docs/shared-app-crashes/replay')
p.add_argument('--source-cache',type=Path,help='Optional JSON mapping repository IDs to local Git object stores')
p.add_argument('--apps',nargs='*',help='Subset for qualification; omitted builds complete selected product')
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);spec=json.loads((a.spec_dir/'inputs.json').read_text());apps=json.loads((a.spec_dir/(a.product+'.json')).read_text())
spec['inputs']={}
for part in spec['input_parts']:spec['inputs'].update(json.loads((a.spec_dir/part).read_text()))
if a.apps:apps={n:apps[n] for n in a.apps}
else:
 expected={'wave':17,'twatch':18,'wave-media':1}[a.product]
 assert len(apps)==expected, f'Complete {a.product} replay requires all {expected} app recipes'
sha=lambda b:hashlib.sha256(b).hexdigest()
cc=a.compiler.resolve();assert cc.is_file()
version=subprocess.check_output([cc,'--version'],text=True).splitlines()[0]
assert '8.4.0' in version,version
cache=json.loads(a.source_cache.read_text()) if a.source_cache else {}
paths={'work':out/'work','cc':cc,'cxx':Path(str(cc)[:-3]+'g++')}
for id,row in spec['repositories'].items():
 target=out/'sources'/id;paths[id]=target
 if not target.exists():
  if id in cache:
   data=subprocess.check_output(['git','-C',cache[id],'archive',row['commit']])
   target.mkdir(parents=True);tarfile.open(fileobj=io.BytesIO(data)).extractall(target,filter='data')
  else:
   target.mkdir(parents=True);subprocess.run(['git','init','-q',target],check=True)
   subprocess.run(['git','-C',target,'fetch','--depth=1','https://github.com/'+row['repository']+'.git',row['commit']],check=True)
   subprocess.run(['git','-C',target,'checkout','-q','--detach','FETCH_HEAD'],check=True)
 (target/'.replay-commit').write_text(row['commit']+'\n')
def render(s):
 for k,v in sorted(paths.items(),key=lambda x:-len(x[0])):s=s.replace('@'+k+'@',str(v))
 assert '@work@' not in s and not any('@'+k+'@' in s for k in paths),s
 return s
def module(name,file):
 entry=importlib.util.spec_from_file_location(name,file);m=importlib.util.module_from_spec(entry);entry.loader.exec_module(m);return m
for name,row in spec['helper_files'].items():assert sha((a.spec_dir/name).read_bytes())==row['sha256']
compile_checked=module('checked_xtensa_link',a.spec_dir/'checked_xtensa_link.py').compile_checked
normalize=module('normalize_crash_replay',a.spec_dir/'normalize_xtensa_relocations.py').normalize
compactors={'loader-metadata-v3':module('compact_crash_v3',a.spec_dir/'compact_current_elf.py').compact,'loader-metadata-v4':module('compact_crash_v4',a.spec_dir/'compact_cohort_elf.py').compact}
# Write path-dependent generated includes after all immutable source roots exist.
for name,row in spec['inputs'].items():
 dest=Path(render(name))
 if 'text' in row:
  assert sha(row['text'].encode())==row['template_sha256'];dest.parent.mkdir(parents=True,exist_ok=True);dest.write_text(render(row['text']))
for name,row in spec['inputs'].items():
 if 'copy' in row:
  dest=Path(render(name));dest.parent.mkdir(parents=True,exist_ok=True)
  if row.get('symlink'):
   dest.unlink(missing_ok=True);dest.symlink_to(render(row['copy']))
  else:shutil.copyfile(render(row['copy']),dest)
for name,row in spec['inputs'].items():
 dest=Path(render(name))
 if dest.suffix=='.o':continue
 if 'text' not in row:assert sha(dest.read_bytes())==row['sha256'],name
needed_objects={v for row in apps.values() for v in row['command'] if v.endswith('.o')}
for row in spec['objects']:
 original=row['command'];target=original[original.index('-o')+1]
 if target not in needed_objects:continue
 cmd=[render(v) for v in original];Path(render(target)).parent.mkdir(parents=True,exist_ok=True);subprocess.run(cmd,check=True)
 assert sha(Path(render(target)).read_bytes())==row['sha256'],target
result={'product':a.product,'driver_sha256':sha(Path(__file__).read_bytes()),'spec_sha256':{x.name:sha(x.read_bytes()) for x in sorted(a.spec_dir.iterdir()) if x.is_file()},'repositories':spec['repositories'],'compiler':version,'compiler_sha256':sha(cc.read_bytes()),'source_inputs_verified':True,'hardware_tested':False,'apps':{}}
for name,row in apps.items():
 cmd=[render(v) for v in row['command']];target=Path(cmd[cmd.index('-o')+1]);target.parent.mkdir(parents=True,exist_ok=True)
 link_attempts=compile_checked(cmd,target,out/(name+'.log'))
 normalize(target);compaction=compactors[row['profile']](target,str(cc),debug_path=out/'debug'/(name+'.elf'))
 assert sha(target.read_bytes())==row['sha256'],(name,'final ELF digest mismatch')
 assert target.stat().st_size==row['bytes']
 elf=ELFFile(io.BytesIO(target.read_bytes()));syms=elf.get_section_by_name('.dynsym')
 actual={'imports':sorted(s.name for s in syms.iter_symbols() if s.name and s['st_shndx']=='SHN_UNDEF'),'exports':sorted(s.name for s in syms.iter_symbols() if s.name and s['st_shndx']!='SHN_UNDEF')}
 assert actual==row['symbols'],(name,'symbol inventory mismatch')
 dest=out/(name+'.elf');shutil.copyfile(target,dest)
 result['apps'][name]={'sha256':row['sha256'],'bytes':row['bytes'],'symbols':actual,'file':str(dest),'compaction':compaction,'link_attempts':link_attempts}
 (out/'receipt.json').write_text(json.dumps(result,indent=2)+'\n');print(name,row['sha256'],flush=True)

#!/usr/bin/env python3
"""Full Files and scene ELF lifecycle through actual Runtime; hardware doubled."""
import argparse,hashlib,json,os,shlex,shutil,subprocess
from pathlib import Path
from qualify_files_scene import DEFINES
from portable_file_sharing_build import MODULES
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for key in ('runtime','include','output'):p.add_argument('--'+key,type=Path,required=True)
p.add_argument('--provider-system',type=Path,default=ROOT);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
o=a.output.resolve();o.mkdir(parents=True,exist_ok=False);common=o/'common';common.mkdir();r=a.runtime.resolve();provider=a.provider_system.resolve();fixture=ROOT/'test/files_runtime'
flags=['-O0','-g1','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-missing-field-initializers','-fPIC','-ffunction-sections','-fdata-sections','-I'+str(a.include.resolve()),'-I'+str(ROOT/'Services/scene_profile'),'-I'+str(ROOT/'Services/text_input')]
if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
commands=[];inputs={}
def call(cmd):
 cmd=list(map(str,cmd));res=subprocess.run(cmd,capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},timeout=180);commands.append(dict(command=cmd,returncode=res.returncode,output=res.stdout+res.stderr));(o/'commands.json').write_text(json.dumps(commands,indent=2)+'\n');print(res.stdout+res.stderr,end='');res.check_returncode()
def build(src,label,extra=()):
 obj=o/(label+'.o');dep=o/(label+'.d');cc='c++' if src.suffix=='.cpp' else 'cc'
 call([cc,'-std=c++17' if cc=='c++' else '-std=c11',*flags,*extra,'-MMD','-MF',dep,'-c',src,'-o',obj])
 for n in shlex.split(dep.read_text().replace('\\\n',' ').split(':',1)[1]):
  f=Path(n).resolve();inputs[str(f)]=hashlib.sha256(f.read_bytes()).hexdigest()
 return obj
def shared(path,objects):call(['c++',*flags,'-shared','-Wl,--gc-sections',*objects,'-lm','-o',path])
sources=[fixture/'app.c',ROOT/'Apps/files_scene_background.c',*[ROOT/'lib/PortableApps/src'/n for n in ('PortableNetworkSession.c','PortableFileSetup.c','PortableTimeZone.c','PortableTimeZoneCatalog.c')],ROOT/'lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c',*[ROOT/'lib/RemoteFiles'/(n+'.cpp') for n in (*MODULES,'WebDavClient','WebDavRemoteVolume','PortablePeerFiles')]]
objects=[build(s,'app-'+str(i),['-fvisibility=hidden',*(['-finstrument-functions',*['-D'+d for d in DEFINES]] if i==0 else [])]) for i,s in enumerate(sources)];shared(common/'file_browser.elf',objects)
shared(common/'scene-host.elf',[build(provider/'Services/scene_host/host.c','scene',['-fvisibility=hidden'])]);shared(common/'launcher.elf',[build(fixture/'launcher.c','launcher',['-fvisibility=hidden'])])
for i,(name,cap) in enumerate((('display','display.output'),('touch','input.touch.raw'),('navigation','input.navigation'))):
 shared(common/(name+'.elf'),[build(fixture/'provider.c',name,['-fvisibility=hidden',f'-DPROVIDER_INDEX={i}',f'-DPROVIDER_ID="{name}"',f'-DPROVIDER_CAP="{cap}"','-DPROVIDER_API=1'])])
hardware=build(fixture/'hardware.c','hardware')
inc=['-I'+str(r/p) for p in ('src','lib/ArduinoJson/src','test/drivers/stubs','test')]
rtpaths=('bootstrap/Json.cpp','bootstrap/Board.cpp','bootstrap/Runtime.cpp','runtime/streams/AppStreamSessions.cpp','runtime/streams/ProviderQueueHost.cpp','runtime/drivers/ProviderGraphV2.cpp','runtime/drivers/ProviderModuleV2.cpp','runtime/storage/AppDataFiles.cpp')
rtobjects=[build(s,'runtime-'+str(i),[*inc,'-DRISC_RUNTIME_PROVIDER_CAPACITY=28','-DRISC_RUNTIME_PREFERENCE_STORE=1']) for i,s in enumerate([*[r/'src'/p for p in rtpaths],fixture/'runtime_test.cpp'])]
exe=o/'runtime-test';call(['c++',*flags,'-no-pie','-rdynamic',*rtobjects,hardware,'-ldl','-o',exe])
cases=[];apphash=hashlib.sha256((common/'file_browser.elf').read_bytes()).hexdigest()
for name,w,h,fmt in (('paper',800,480,1),('watch',240,240,5),('amoled',410,502,5)):
 profile=o/(name+'-profile.elf');shared(profile,[build(provider/'Services/scene_profile/profile.c',name+'-profile',['-fvisibility=hidden',*(['-DSCENE_PROFILE_PAPER=1'] if fmt==1 else [])])])
 for mode in ('demand','retained-providers','no-ui'):
  target=o/(name+'-'+mode);shutil.copytree(common,target);shutil.copyfile(profile,target/'scene-presentation-profile.elf');assert hashlib.sha256((target/'file_browser.elf').read_bytes()).hexdigest()==apphash
  call([exe,target,w,h,mode]);cases.append(dict(profile=name,mode=mode,passed=True))
assert inputs=={p:hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in inputs}
(o/'receipt.json').write_text(json.dumps(dict(scope=__doc__,cases=cases,sanitized=a.sanitize,compiled_inputs=inputs,app_sha256=apphash,hardware_tested=False,actual_runtime=True),indent=2)+'\n')

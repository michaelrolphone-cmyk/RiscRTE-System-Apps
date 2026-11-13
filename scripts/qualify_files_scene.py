#!/usr/bin/env python3
"""Reproducible Files reconstruction preparation; never claims a linked app."""
import argparse, hashlib, json, os, shutil, subprocess
from pathlib import Path
from scene_sdk import stage_sdk
from native_toolchain import compiler_path
ROOT = Path(__file__).resolve().parents[1]
DEFINES = ['PORTABLE_FILE_BROWSER_SHARED_INPUT','PORTABLE_TEXT_INPUT_CLIENT',
 'PORTABLE_TEXT_INPUT_POLICY','PORTABLE_FILE_BROWSER_HANDLERS',
 'PORTABLE_FILE_BROWSER_SOURCE_HANDOFF','PORTABLE_FILE_SHARING',
 'PORTABLE_FILE_SHARING_SELECTED_VOLUME','PORTABLE_FILE_PEER','PORTABLE_FILE_SETUP',
 'PORTABLE_NATIVE_CUSTODY_FENCE','PORTABLE_ALARM_TERMINAL_RETENTION',
 'PORTABLE_CONTEXTS_CLIENT','PORTABLE_BLE_BROADCAST',
 'PORTABLE_FILE_SHARING_EXPORT_INSTANCE=0','PORTABLE_FILE_SHARING_TCP_INSTANCE=0',
 'PORTABLE_FILE_SHARING_ENTROPY_INSTANCE=0','PORTABLE_FILE_SHARING_SETUP_INSTANCE=0',
 'PORTABLE_FILE_BROWSER_SECONDARY_CAPABILITY="storage.app-data.export"',
 'PORTABLE_FILE_BROWSER_SECONDARY_INSTANCE=0',
 'PORTABLE_FILE_BROWSER_TERTIARY_CAPABILITY="storage.volume"',
 'PORTABLE_FILE_BROWSER_TERTIARY_INSTANCE=0']
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--utilities',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--target-cc');a=p.parse_args()
 out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);runtime=a.runtime.resolve();records=[]
 def run(cmd,env=None):
  r=subprocess.run(list(map(str,cmd)),cwd=ROOT,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
  records.append({'command':list(map(str,cmd)),'returncode':r.returncode,'output':r.stdout})
  (out/'commands.json').write_text(json.dumps(records,indent=2)+'\n')
  print(r.stdout,end='');r.check_returncode();return r.stdout
 # One physical copy of each header per translation unit. Public Runtime and
 # System SDK contracts deliberately supersede bundled historical copies.
 inc=out/'include';inc.mkdir(exist_ok=True);origins={}
 for folder in (*(a.utilities/'lib'/x/'include' for x in ('Alarm','Bluetooth','Contexts')),a.utilities/'Apps',ROOT/'lib/NativeApps/include',ROOT/'lib/PortableApps/include',*(runtime/'sdk'/x for x in ('app','driver','hardware')),ROOT/'sdk/app'):
  for f in folder.glob('*.h'):
   shutil.copyfile(f,inc/f.name);origins[f.name]={'path':str(f),'sha256':hashlib.sha256(f.read_bytes()).hexdigest()}
 (out/'header-origins.json').write_text(json.dumps(origins,indent=2)+'\n')
 cc=os.environ.get('CC','cc');flags=['-std=c11','-Wall','-Wextra','-Werror','-I'+str(inc)]
 env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0')
 for name in ('presenter','text_pending','ui','root_text','handoff','resident','effects'):
  for sanitized in (False,True):
   exe=out/(name+('-san' if sanitized else ''));extra=[]
   if name=='root_text':extra=['-ffunction-sections','-fdata-sections','-Wl,--gc-sections',*['-D'+x for x in DEFINES]]
   if sanitized:extra+=['-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
   run([cc,*flags,*extra,'-MMD','-MF',str(exe)+'.d',ROOT/f'test/files_scene/{name}_test.c','-o',exe]);run([exe],env)
 provider_inc=stage_sdk(runtime,ROOT,out/'provider-sdk');exe=out/'provider-configure'
 run([cc,'-std=c11','-I'+str(provider_inc),'-MM','-MF',out/'provider-source.d',ROOT/'Services/scene_host/host.c'])
 run([cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(provider_inc),'-MMD','-MF',str(exe)+'.d',ROOT/'Services/scene_host/host.c',ROOT/'test/files_scene/provider_configure_test.c','-o',exe])
 for profile in ('watch','paper','gray'):run([exe,profile])
 for compiler,name,extra in ((cc,'host',[]),(compiler_path(a.target_cc),'xtensa',['-Os','-fPIC','-mtext-section-literals','-mlongcalls','-ffunction-sections','-fdata-sections','-ffreestanding','-fno-builtin'])):
  run([compiler,*flags,*extra,*['-D'+x for x in DEFINES],'-MMD','-MF',out/(name+'.d'),'-c',ROOT/'Apps/files_scene.c','-o',out/(name+'.o')])
 # Parse both authorized alarm API variants using the actual Runtime parser.
 from link_files_scene import make_manifest
 manifest_test=out/'manifest-test'
 run([os.environ.get('CXX','c++'),'-std=c++17','-Wall','-Wextra','-Werror',
      '-I'+str(runtime/'src'),'-I'+str(runtime/'sdk/app'),'-I'+str(runtime/'sdk/driver'),
      '-I'+str(runtime/'lib/ArduinoJson/src'),'-MMD','-MF',out/'manifest-test.d',
      ROOT/'test/files_scene/manifest_test.cpp','-o',manifest_test])
 for api in (1,2):
  manifest=out/('manifest-alarm-'+str(api)+'.json')
  manifest.write_text(json.dumps(make_manifest('0.0.0',api),indent=2)+'\n')
  run([manifest_test,manifest])
 # Compile the real sharing/network/setup implementation separately. These
 # objects remain unlinked until the owned background seam is available.
 target_cc=compiler_path(a.target_cc)
 target_flags=['-Os','-fPIC','-mtext-section-literals','-mlongcalls','-ffunction-sections','-fdata-sections','-fvisibility=hidden','-ffreestanding','-fno-builtin','-Wall','-Wextra','-Werror','-I'+str(inc)]
 from portable_file_sharing_build import MODULES
 for name in (*MODULES,'WebDavClient','WebDavRemoteVolume','PortablePeerFiles'):
  run([target_cc.removesuffix('gcc')+'g++','-std=c++17','-fno-exceptions','-fno-rtti',*target_flags,'-MMD','-MF',out/(name+'.d'),'-c',ROOT/('lib/RemoteFiles/'+name+'.cpp'),'-o',out/(name+'.o')])
 for source in ('lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c','lib/PortableApps/src/PortableNetworkSession.c','lib/PortableApps/src/PortableFileSetup.c','Apps/files_scene_background.c','lib/NativeApps/src/UnsignedDivisionCompat.c','lib/PortableApps/src/PortableTimeZone.c','lib/PortableApps/src/PortableTimeZoneCatalog.c'):
  name=Path(source).stem
  run([target_cc,'-std=c11','-Wno-unused-function',*target_flags,'-MMD','-MF',out/(name+'.d'),'-c',ROOT/source,'-o',out/(name+'.o')])
 deps={}
 for d in out.glob('*.d'):
  for token in d.read_text().replace('\\\n',' ').split()[1:]:
   f=Path(token);f=f if f.is_absolute() else ROOT/f
   if f.is_file():deps[str(f)]=hashlib.sha256(f.read_bytes()).hexdigest()
 receipt={'status':'preparation only: root object compiled; full app not linked','hardware_tested':False,
  'runtime_commit':run(['git','-C',runtime,'rev-parse','HEAD']).strip() if (runtime/'.git').exists() else None,
  'system_commit':run(['git','rev-parse','HEAD']).strip() if (ROOT/'.git').exists() else None,'preparation_script_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'defines':DEFINES,'dependency_sha256':deps,
  'pending':['Full root background/modal/settings behavior qualification','Full app link, ABI validation, repeat ELF build','Adversarial full-root/provider tests and hardware acceptance']}
 (out/'qualification.json').write_text(json.dumps(receipt,indent=2)+'\n')
 print(receipt['status'])
if __name__=='__main__':main()

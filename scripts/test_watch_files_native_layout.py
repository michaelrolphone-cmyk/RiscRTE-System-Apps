import subprocess,argparse
from pathlib import Path
r=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--peer-objects',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
objects=[p for p in a.peer_objects.resolve().glob('*.o') if p.name not in ('rgb.o','paper.o','paper-scroll.o','paper-scroll-snapshot.o','text-host.o')];assert len(objects)==15
flags=['-O1','-Wall','-Wextra','-Werror','-Wno-unused-function','-fsanitize=undefined','-fno-sanitize-recover=all','-DPORTABLE_FILE_SHARING_SELECTED_VOLUME','-DPORTABLE_WATCH_NATIVE_HEIGHT','-DFILE_TEST_WIDTH=410u','-DFILE_TEST_HEIGHT=502u',*['-I'+str(r/x) for x in ['lib/PortableApps/include','lib/NativeApps/include','lib/RemoteFiles','lib/UserFiles']],'-I'+str(a.runtime.resolve()/'src'),'-I'+str(a.runtime.resolve()/'sdk/app')]
for name in ['file_browser_peer_test','file_browser_ui_parity_test']:
 obj=out/(name+'.o');binary=out/name
 subprocess.run(['/usr/bin/cc','-std=c11',*flags,'-c',str(r/'test/native_apps'/ (name+'.c')),'-o',str(obj)],check=True)
 subprocess.run(['/usr/bin/c++','-fsanitize=undefined',str(obj),*map(str,objects),'-o',str(binary)],check=True)
 if name.endswith('parity_test'):
  frames=out/'native-screens';frames.mkdir(exist_ok=True);subprocess.run([str(binary),str(frames)],check=True,stdout=subprocess.DEVNULL)
 else:
  for case in ['direct','transfer','upload','existing','cancel','expiry','unavailable','owned']:
   d=out/case;d.mkdir(exist_ok=True);result=subprocess.run([str(binary),case,str(d)],text=True,capture_output=True);print(case,result.returncode,result.stdout[-350:],result.stderr[-350:],flush=True);assert result.returncode==0

for name in ('files_native_layout_test','quick_native_layout_test'):
 sources=[r/'test/native_apps'/(name+'.c')]
 if name.startswith('quick'):sources += [r/'lib/PortableApps/src/quick_actions.c',r/'lib/PortableApps/src/quick_render.c']
 binary=out/name
 subprocess.run(['/usr/bin/cc','-std=c11',*flags,*map(str,sources),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)

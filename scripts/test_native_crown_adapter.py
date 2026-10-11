"""Run production shared adapter with persistent native exit and legacy tables."""
import argparse,os,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--utilities',type=Path,required=True);a=p.parse_args()
with tempfile.TemporaryDirectory() as t:
 for name,source,flags in [('springboard',R/'Apps/springboard.c',[]),('battery',a.utilities/'Apps/battery.c',['-DBATTERY_TEST'])]:
  out=Path(t)/name
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-fsanitize=undefined',*flags,'-I'+str(R/'lib/PortableApps/include'),'-I'+str(R/'lib/NativeApps/include'),str(source),str(R/'lib/PortableApps/src/adapter.c'),str(R/'test/native_apps/portable_adapter_test.c'),'-o',str(out)],check=True)
  for mode in (0,2,4,8):subprocess.run([str(out),str(mode)],check=True,timeout=10)

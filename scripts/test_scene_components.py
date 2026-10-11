#!/usr/bin/env python3
import argparse,os,subprocess,tempfile
from pathlib import Path
from scene_sdk import stage_sdk
ROOT=Path(__file__).resolve().parents[1]
def run(runtime):
 with tempfile.TemporaryDirectory(prefix='nova-components-') as temporary:
  out=Path(temporary);include=stage_sdk(runtime,ROOT,out);exe=out/'components'
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(include),str(ROOT/'Services/scene_host/host.c'),str(ROOT/'test/scene/components_test.c'),'-o',str(exe)],check=True)
  for profile in ('watch','paper'):subprocess.run([str(exe),profile],check=True)
  page=out/'page'
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(include),str(ROOT/'Services/scene_host/host.c'),str(ROOT/'test/scene/page_test.c'),'-o',str(page)],check=True)
  subprocess.run([str(page)],check=True)
  reading=out/'reading'
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(include),str(ROOT/'Services/scene_host/host.c'),str(ROOT/'test/scene/reading_test.c'),'-o',str(reading)],check=True)
  subprocess.run([str(reading)],check=True)
  links=out/'reading-links'
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(include),str(ROOT/'Services/scene_host/host.c'),str(ROOT/'test/scene/reading_links_test.c'),'-o',str(links)],check=True)
  subprocess.run([str(links)],check=True)
  checkpoint=out/'checkpoint'
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(include),str(ROOT/'Services/scene_host/host.c'),str(ROOT/'test/scene/checkpoint_test.c'),'-o',str(checkpoint)],check=True)
  for mode in ('static','external','raster','pending','paused-close','unsubscribe-fault','subscribe','snapshot','foreground','reset'):subprocess.run([str(checkpoint),mode],check=True)
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);a=p.parse_args();run(a.runtime.resolve())

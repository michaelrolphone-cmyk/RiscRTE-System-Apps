#!/usr/bin/env python3
"""Real Settings controller/raster/hit tests at varied native sizes, not hardware."""
import argparse,hashlib,json,os,subprocess,tempfile
from pathlib import Path
S=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--native-settings-ui',action='store_true');p.add_argument('--output',type=Path,required=True);a=p.parse_args();o=a.output.resolve();o.mkdir(parents=True,exist_ok=True);rows=[]
 with tempfile.TemporaryDirectory(prefix='settings-timer-geometry-') as tmp:
  for w,h,inset,rotation in [(240,240,0,0),(300,300,0,0),(466,466,68,0),(466,466,68,180),(400,480,30,0)]:
   for san in [False,True]:
    name=f'{w}x{h}-r{rotation}-'+('sanitized' if san else 'normal');frames=o/'frames';capture=w==466 and rotation==0 and not san
    flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-DPORTABLE_SETTINGS_BOTH_TIMERS','-DPORTABLE_SAFE_VIEWPORT',f'-DTEST_SETTINGS_RASTER_WIDTH={w}',f'-DTEST_SETTINGS_RASTER_HEIGHT={h}',f'-DTEST_SETTINGS_INSET={inset}',f'-DPORTABLE_TOUCH_ROTATION={rotation}']
    if a.native_settings_ui:flags+=['-DPORTABLE_SETTINGS_NATIVE_GEOMETRY']
    if capture:flags+=['-DTEST_SETTINGS_FRAME_CAPTURE'];frames.mkdir(exist_ok=True)
    if san:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
    exe=Path(tmp)/'test';subprocess.run(['cc',*flags,'-I'+str(S/'lib/PortableApps/include'),'-I'+str(S/'lib/NativeApps/include'),str(S/'Apps/settings.c'),str(S/'test/native_apps/settings_sleep_defaults_test.c'),'-o',str(exe)],check=True)
    env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'}
    if capture:env['SETTINGS_TIMER_FRAME_DIR']=str(frames)
    r=subprocess.run([str(exe)],text=True,capture_output=True,env=env);(o/(name+'.log')).write_text(r.stdout+r.stderr);assert r.returncode==0,r.stdout+r.stderr;rows.append({'width':w,'height':h,'inset':inset,'rotation':rotation,'sanitize':san,'passed':True});print(name,'PASS',flush=True)
    if capture:
     from PIL import Image
     for source in frames.glob('*.ppm'):
      im=Image.open(source);dest=source.with_suffix('.png');im.save(dest);assert Image.open(dest).tobytes()==im.tobytes();source.unlink()
 files=[S/'lib/PortableApps/src/settings.inc',S/'lib/PortableApps/src/settings_view.inc',S/'test/native_apps/portable_settings_test.c',S/'test/native_apps/settings_sleep_defaults_test.c',Path(__file__)]
 (o/'receipt.json').write_text(json.dumps({'passed':True,'physical':False,'cases':rows,'source_sha256':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files}},indent=2)+'\n')
if __name__=='__main__':main()

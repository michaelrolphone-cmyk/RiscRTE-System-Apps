from pathlib import Path
import os,subprocess,json,hashlib,time
root=Path('/tmp/quick-native-adapter-20261017')
out=root/'build/paper-label-targets';out.mkdir(exist_ok=True)
cc='/workspace/scratch/c744abbbbd60/watch-build-tools/platformio-core/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc'
env=dict(os.environ,NATIVE_APP_CC=cc)
jobs=[
 ('wifi',['scripts/build_portable_wifi.py','--nova-ui','--alarm-client','--navigation','--quick-actions','--wall-time','--wifi-instance','15','--display-rotation','90','--return-app','springboard.elf','--home-app','default.elf']),
 ('clock',['scripts/build_paper_clock.py','--quick-actions','--alarm-client','--navigation']),
 ('file-browser',['scripts/build_portable_file_browser.py','--quick-controls','--alarm-client','--navigation']),
 ('settings',['scripts/build_native_settings_qualification.py','--bridges','/tmp/native-settings-target-20261017/bridges','--runtime','/tmp/waveshare-provisioning/runtime','--profile-source','/tmp/waveshare-provisioning/product','--utilities','/tmp/x4-settings-producer-20261016/utilities-history'])]
results=[]
for name,args in jobs:
 destination=out/name;command=['python',*args,'--output-dir',str(destination)]
 start=time.time()
 with (out/(name+'.log')).open('w') as log:
  run=subprocess.run(command,cwd=root,env=env,stdout=log,stderr=subprocess.STDOUT)
 record={'name':name,'command':command,'returncode':run.returncode,'seconds':time.time()-start,'executable_sha256':{}}
 for path in destination.rglob('*'):
  if path.is_file() and path.read_bytes().startswith(b'\x7fELF'):
   record['executable_sha256'][str(path)]=hashlib.sha256(path.read_bytes()).hexdigest();path.unlink()
 results.append(record)
 (out/'results.json').write_text(json.dumps({'source_head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),'compiler':cc,'compiler_sha256':hashlib.sha256(Path(cc).read_bytes()).hexdigest(),'results':results},indent=2)+'\n')
 print(name,run.returncode,flush=True)
 if run.returncode:raise SystemExit(run.returncode)

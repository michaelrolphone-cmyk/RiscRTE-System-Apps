from pathlib import Path
import os,subprocess,json,hashlib,time
r=Path('/workspace/shared/x4-settings-system-101-20261012');e=Path(__file__).resolve().parent
runtime='/workspace/shared/x4-https-099-20261012/runtime';home='/workspace/shared/x4-home-automatic-target-on'
commands=[('alarm-busy-adapter',['python3','scripts/test_alarm_busy_adapter.py','--runtime',runtime,'--utilities','/workspace/shared/first-attempt-time-alarm-utilities-20261012','--output',str(e/'alarm-busy-adapter')],{})]
for san in [False,True]:
 mode='asan-ubsan' if san else 'normal';flag=['--sanitize'] if san else []
 for script,extra in [
  ('test_wifi_atomic.py',[]),
  ('test_settings_atomic.py',['--target',str(e/'settings-fixture-target')]),
  ('test_quick_atomic.py',['--target',home]),
  ('test_home_automatic.py',['--target',home]),
  ('test_text_input_host.py',['--runtime',runtime,'--decoupled-attention'])]:
  name=script[5:-3]+'-'+mode;commands.append((name,['python3','scripts/'+script,*extra,'--output',str(e/name),*flag],{}))
 commands.append(('scene-host-'+mode,['python3','scripts/test_scene_host.py','--runtime',runtime,*flag],{}))
 for script in ['test_file_browser_peer.sh','test_file_browser_mount.sh','test_portable_file_setup.sh','test_file_browser_source.sh','test_file_browser_local_scopes.sh']:
  name=script[5:-3]+(('-asan-ubsan' if script=='test_file_browser_source.sh' else '-ubsan') if san else '-normal');commands.append((name,['bash','scripts/'+script],{'SANITIZE':str(int(san)),'OUTPUT_DIR':str(e/name)}))
results=[]
for name,command,extra in commands:
 log=e/(name+'.log');started=time.time();print('START',name,flush=True)
 with log.open('w') as f:
  f.write(json.dumps({'command':command,'cwd':str(r),'environment':extra})+'\n');f.flush()
  result=subprocess.run(command,cwd=r,stdout=f,stderr=subprocess.STDOUT,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1',**extra})
 row={'name':name,'command':command,'environment':extra,'exit_code':result.returncode,'seconds':round(time.time()-started,3),'log_sha256':hashlib.sha256(log.read_bytes()).hexdigest()};results.append(row)
 (e/'regressions.json').write_text(json.dumps({'results':results,'complete':False,'hardware_tested':False},indent=2)+'\n');print('END',name,result.returncode,flush=True)
 if result.returncode:raise SystemExit(result.returncode)
start=json.loads((e/'source-start.json').read_text());assert all(hashlib.sha256((r/f).read_bytes()).hexdigest()==h for f,h in start['sha256'].items())
(e/'regressions.json').write_text(json.dumps({'results':results,'complete':True,'source_unchanged':True,'hardware_tested':False},indent=2)+'\n')

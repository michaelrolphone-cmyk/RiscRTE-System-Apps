from concurrent.futures import ThreadPoolExecutor
import hashlib,json,os,subprocess
from pathlib import Path
base=Path('/workspace/shared/watch-launch-brightness-20261015');root=base/'system-consolidation';out=base/'system-union-qualification';py='/workspace/scratch/c744abbbbd60/watch-build-tools/python/bin/python';util=Path('/workspace/shared/watch-sdr-native-release-20261015/wave-source/utilities');sdk=base/'wave-source-qualification/sdk';fullsdk=base/'system-union-atomic-sdk'
commands={
'springboard-final':[py,'scripts/test_springboard_nova.py'],
'contexts-x4-final':[py,'scripts/test_context_x4_adapter.py','--sdk-include',sdk,'--app-data-sdk',sdk],
'file-policy-final':[py,'scripts/test_file_browser_policy.py','--utilities',util,'--output',out/'file-policy-final','--legacy'],
}
for name in ('quick_atomic','home_automatic'):
 for sanitized in (False,True):
  label=name+('-sanitized' if sanitized else '-normal')+'-final'
  commands[label]=[py,'scripts/test_'+name+'.py','--sdk',fullsdk,'--output',out/label]+(['--sanitize'] if sanitized else [])
env={**os.environ,'PYTHONDONTWRITEBYTECODE':'1','ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1','RISC_RUNTIME_SDK':'/workspace/shared/watch-sdr-native-release-20261015/wave-source/runtime/sdk/app','CPATH':str(util/'Apps')+':'+str(util/'lib/Alarm/include')}
def test(item):
 label,cmd=item;cmd=list(map(str,cmd));log=out/(label+'.log')
 with log.open('w') as stream:proc=subprocess.run(cmd,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT)
 row={'label':label,'command':cmd,'exit':proc.returncode,'log_sha256':hashlib.sha256(log.read_bytes()).hexdigest()};print(label,proc.returncode,flush=True);return row
commands={name+'-v7':[str(arg).replace(name,name+'-v7') for arg in args] for name,args in commands.items() if name.startswith('quick_atomic')}
with ThreadPoolExecutor(max_workers=2) as pool:results=list(pool.map(test,commands.items()))
(out/'atomic-final-results.json').write_text(json.dumps({'environment':{k:env[k] for k in ('ASAN_OPTIONS','UBSAN_OPTIONS','RISC_RUNTIME_SDK','CPATH')},'files_runtime_revision':'64c8e52e46fd91e8fdafd636db04eed88f0c51a9','results':results},indent=2)+'\n')
raise SystemExit(any(x['exit'] for x in results))

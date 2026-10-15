from concurrent.futures import ThreadPoolExecutor
import hashlib,json,os,subprocess,sys
from pathlib import Path
base=Path('/workspace/shared/watch-launch-brightness-20261015');root=base/'system-consolidation';out=base/'system-union-qualification';out.mkdir(exist_ok=True)
py='/workspace/scratch/c744abbbbd60/watch-build-tools/python/bin/python'
sdk=base/'wave-source-qualification/sdk';util=Path('/workspace/shared/watch-sdr-native-release-20261015/wave-source/utilities')
common=['--sdk-include',sdk,'--app-data-sdk',sdk,'--utilities-root',util]
commands={
 'unittests':[py,'-m','unittest','discover','-s','tests','-v'],
 'custody':[py,'scripts/test_shared_data_custody.py',*common,'--callback-audit','--output',out/'custody'],
 'brightness':[py,'scripts/test_shared_initial_brightness.py',*common,'--output',out/'brightness'],
 'rf-native':[py,'scripts/test_rf_native_raster.py','--output',out/'rf-native'],
}
ui=['test_apps','test_portable_apps','test_portable_wifi','test_portable_file_browser','test_file_browser_paper','test_file_browser_operations','test_file_browser_appdata_export','test_portable_update','test_portable_alarm','test_nova_ui','test_nova_settings','test_quick_actions','test_quick_native','test_quick_radios','test_portable_audio','test_portable_radio','test_portable_alarm_settings','test_springboard_nova','test_springboard_paper','test_settings_paper','test_nova_launcher','test_portable_transition','test_paper_quick_actions']
commands.update({name:[py,'scripts/'+name+'.py'] for name in ui})
commands['oversize-usb']=[py,'test/native_apps/file_browser_oversize_usb_handle_runtime_test.py']
commands['quick-raster-parity']=[py,'scripts/test_quick_raster_rows.py','--baseline','/workspace/shared/watch-launch-brightness-20261015/system-quick-baseline','--output',out/'quick-parity','--sanitize']
fullsdk=base/'system-union-sdk'
for label in ('test_context_client','test_context_preferences'):
 commands[label]=[py,'scripts/'+label+'.py']
commands['contexts-x4']=[py,'scripts/test_context_x4_adapter.py','--sdk-include',sdk,'--app-data-sdk',sdk]
commands['file-policy']=[py,'scripts/test_file_browser_policy.py','--utilities',util,'--output',out/'file-policy','--legacy']
commands['text-continuity']=[py,'scripts/test_text_input_handoff_continuity.py','--runtime','/workspace/shared/watch-sdr-native-release-20261015/wave-source/runtime','--output',out/'text-continuity','--sanitize']
for name in ('quick_atomic','home_automatic'):
 for sanitized in (False,True):
  label=name+('-sanitized' if sanitized else '-normal')
  commands[label]=[py,'scripts/test_'+name+'.py','--sdk',fullsdk,'--output',out/label]+(['--sanitize'] if sanitized else [])
# Exact predecessor Quick source and the current capability SDK are staged above.
env={**os.environ,'PYTHONDONTWRITEBYTECODE':'1','ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'}
def test(item):
 label,cmd=item;cmd=[str(v) for v in cmd];log=out/(label+'.log')
 with log.open('w') as stream:
  proc=subprocess.run(cmd,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT)
 row={'label':label,'command':cmd,'exit':proc.returncode,'log_sha256':hashlib.sha256(log.read_bytes()).hexdigest()}
 print(label,proc.returncode,flush=True)
 return row
with ThreadPoolExecutor(max_workers=4) as pool:results=list(pool.map(test,commands.items()))
(out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
raise SystemExit(any(x['exit'] for x in results))

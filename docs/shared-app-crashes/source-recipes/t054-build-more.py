from pathlib import Path
import json,subprocess,sys,hashlib
R=Path.cwd();out=R/'t054-build/wave-components';old=Path('/Users/micahelbyrns/Documents/Codex/2026-10-12/task')
sys.path.insert(0,str(R/'Shared-t054/scripts'));from normalize_xtensa_relocations import normalize
from compact_current_elf import compact
sources={'app_store':old/'ota-provisioning-0112/native-apps-clean/app_store-build.json','ota_update':old/'ota-provisioning-0112/native-apps-clean/ota_update-build.json','wifi_settings':R/'t051-product-build/waveshare-wifi-command.json','audio_spectrum':R/'Wave-t054/docs/t069/audio_spectrum-command.json','waterfall':R/'Wave-t054/integration/sdr-startup/waterfall-build-command.json','timecard':old/'cumulative-timecard-0120/target-color/command.json','points_in_time':R/'t046-build/points-wave/build.json','file_browser':R/'t052-build/waveshare/command.json','ble_scanner':R.parent/'task-2/watch-builds/build-recipes/ble_scanner-command.json','lora_messages':R.parent/'task-2/watch-builds/build-recipes/lora_messages-command.json'}
proof={}
for name,recipe in sources.items():
 if name=='ble_scanner':continue
 x=json.load(open(recipe));c=x['command'] if isinstance(x,dict) else x
 previous=next(v[:-len('/lib/PortableApps/src/adapter.c')] for v in c if v.endswith('/lib/PortableApps/src/adapter.c'))
 c=[v.replace(previous,str(R/'System-product-t054')) for v in c]
 c.insert(1,'-I'+str(R/'System-product-t054/lib/PortableApps/include'))
 c.insert(2,'-I'+str(R/'System-product-t054/Apps'))
 
 if name=='waterfall':
  h=out/'rf-presentation.h';h.write_text('#include \"'+str(R/'System-product-t054/Apps/PaperPresentation.h')+'\"\n#define RF_PRESENTATION_H\n');c=[str(h) if v.endswith('/rf-presentation.h') else v for v in c]
 c[c.index('-o')+1]=str(out/(name+'.elf'));c=[v if not v.startswith('-Wl,--version-script=') else '-Wl,--version-script='+str(out/(name+'.map')) for v in c]
 (out/(name+'.map')).write_text('{ global: app_main; app_module_init; app_module_fini; local: *; };\n')
 for flag in ['PORTABLE_QUICK_ATOMIC_PREFERENCES','PORTABLE_HOME_AUTOMATIC_PREFERENCES','PORTABLE_NATIVE_CUSTODY_FENCE','PORTABLE_CHECKED_STORAGE_RUNTIME','PORTABLE_WATCH_COLOR_ONLY']:
  if '-D'+flag not in c:c.append('-D'+flag)
 (out/(name+'-command.json')).write_text(json.dumps(c,indent=2)+'\n')
 with (out/(name+'.log')).open('w') as log:
  result=subprocess.run(c,stdout=log,stderr=subprocess.STDOUT)
 if result.returncode:print(name,'COMPILE FAILED',flush=True);proof[name]={'compile_failed':True};continue
 normalize(out/(name+'.elf'));receipt=compact(out/(name+'.elf'),c[0],debug_path=out/'debug'/(name+'.elf'));proof[name]=receipt
 (out/'more-proof.json').write_text(json.dumps(proof,indent=2)+'\n');print(name,receipt['after_bytes'],flush=True)

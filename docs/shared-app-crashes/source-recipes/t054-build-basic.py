from pathlib import Path
import json,subprocess,sys,hashlib
R=Path.cwd();out=R/'t054-build/wave-components';out.mkdir(exist_ok=True)
sys.path.insert(0,str(R/'Shared-t054/scripts'));from normalize_xtensa_relocations import normalize
from compact_current_elf import compact
proof={}
for name in ['battery','calculator','stopwatch','frequency_generator','alarms','countdown','ble_touchpad','ble_buttons']:
 c=json.load(open(R/'Wave-t054/integration/ordered-input'/(name+'-command.json')))
 c=[v.replace(str(R/'System-input-decoupling'),str(R/'System-product-t054')).replace(str(R.parent/'task-2/utilities-current'),str(R/'Utilities-rgb-t054')) for v in c]
 c[c.index('-o')+1]=str(out/(name+'.elf'))
 c=[v if not v.startswith('-Wl,--version-script=') else '-Wl,--version-script='+str(out/(name+'.map')) for v in c]
 (out/(name+'.map')).write_text('{ global: app_main; app_module_init; app_module_fini; local: *; };\n')
 c+=['-DPORTABLE_QUICK_ATOMIC_PREFERENCES','-DPORTABLE_HOME_AUTOMATIC_PREFERENCES','-DPORTABLE_NATIVE_CUSTODY_FENCE','-DPORTABLE_CHECKED_STORAGE_RUNTIME','-DPORTABLE_WATCH_COLOR_ONLY','-DPORTABLE_UTILITIES_ATOMIC_PREFERENCES','-DPORTABLE_BROADCAST_RESULT_ONLY']
 (out/(name+'-command.json')).write_text(json.dumps(c,indent=2)+'\n')
 with (out/(name+'.log')).open('w') as log:subprocess.run(c,check=True,stdout=log,stderr=subprocess.STDOUT)
 normalize(out/(name+'.elf'));receipt=compact(out/(name+'.elf'),c[0],debug_path=out/'debug'/(name+'.elf'));proof[name]=receipt
 (out/'basic-proof.json').write_text(json.dumps(proof,indent=2)+'\n');print(name,receipt['after_bytes'],flush=True)

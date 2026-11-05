"""Production renderer timing with an explicit simulated 10ms app poll period."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
from scene_sdk import stage_sdk
R=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--baseline',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);inc=stage_sdk(a.runtime.resolve(),R,out/'sdk');proof={}
for name,root in [('baseline',a.baseline.resolve()),('candidate',R)]:
 exe=out/name;subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(inc),'-I'+str(R/'test/scene'),str(root/'Services/scene_host/host.c'),str(R/'test/scene/scroll_timing_test.c'),'-o',str(exe)],check=True)
 raw=out/(name+'.rgb565');proof[name]=json.loads(subprocess.check_output([str(exe),str(raw)],text=True));proof[name]['sha256']=hashlib.sha256(raw.read_bytes()).hexdigest();proof[name]['source_revision']=subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip();proof[name]['sources']={str(f.relative_to(root)):hashlib.sha256(f.read_bytes()).hexdigest() for f in [root/'Services/scene_host/host.c',root/'Services/scene_host/components.inc']}
assert proof['baseline']['sha256']==proof['candidate']['sha256']
assert proof['candidate']['ticks_per_frame']<proof['baseline']['ticks_per_frame']
proof['limits']=['Host measurement of production renderer, not device CPU or panel measurement.','10 ms is an explicitly simulated app poll period; Contexts production loop requests 10 ms.','No physical touch latency, RF load or light-sleep claim.']
(out/'timing.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps(proof,indent=2))

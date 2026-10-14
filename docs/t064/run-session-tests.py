from pathlib import Path
import subprocess,hashlib,json,os
root=Path('/tmp/System-t054');runtime=Path('/tmp/Runtime-wave-t054');out=Path('/tmp/t064-session-proof');out.mkdir(exist_ok=True);runs=[]
for mode in ['normal','sanitized']:
 flags=[] if mode=='normal' else ['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 cmd=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-I'+str(runtime/'sdk/app'),'-I'+str(root/'lib/PortableApps/include'),str(root/'test/native_apps/crash_report_session_test.c'),'-o',str(out/mode)]
 subprocess.run(cmd,check=True);r=subprocess.run([str(out/mode)],capture_output=True,text=True,check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});(out/(mode+'.log')).write_text(r.stdout+r.stderr);runs.append(dict(command=cmd,output=r.stdout,exit=r.returncode));print(r.stdout)
(out/'receipt.json').write_text(json.dumps(dict(runs=runs,source_sha256={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [root/'lib/PortableApps/include/CrashReportSession.h',root/'test/native_apps/crash_report_session_test.c']}),indent=2)+'\n')

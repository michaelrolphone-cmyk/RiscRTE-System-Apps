#!/usr/bin/env python3
"""Check the exact production attention body in local and shared navigation modes."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--no-leak-checks',action='store_true');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
 source=ROOT/'lib/PortableApps/src/text_input_handoff.inc';text=source.read_text();start=text.index('int portable_text_adapter_attention(void) {');end=text.index('\nint portable_text_adapter_resume',start);body=text[start:end];(out/'attention-body.inc').write_text(body)
 rows=[]
 for san in [False,True]:
  for local in [False,True]:
   for policy in [False,True]:
    for alarm in [False,True]:
     flags=['-DPORTABLE_INPUT_NAVIGATION']+(['-DPORTABLE_INPUT_NAVIGATION_LOCAL'] if local else [])+(['-DPORTABLE_TEXT_INPUT_POLICY'] if policy else [])+(['-DPORTABLE_ALARM_CLIENT'] if alarm else [])
     if san:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-fno-pie','-no-pie']
     binary=out/'attention';cmd=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable',*flags,'-I'+str(out),'-I'+str(ROOT/'lib/PortableApps/include'),str(ROOT/'test/text_input/local_navigation_attention_test.c'),'-o',str(binary)];subprocess.run(cmd,check=True)
     env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1'}
     if a.no_leak_checks:env['ASAN_OPTIONS']='detect_leaks=0'
     run=subprocess.run([str(binary)],capture_output=True,text=True,env=env,timeout=30);rows.append(dict(sanitized=san,local=local,policy=policy,alarm=alarm,exit_code=run.returncode,stdout=run.stdout,stderr=run.stderr));assert not run.returncode,run.stderr;binary.unlink()
 result=dict(schema='text.local-navigation-attention.v1',production_source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),production_body_sha256=hashlib.sha256(body.encode()).hexdigest(),runs=rows,hardware_verified=False);(out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(len(rows),'production attention configurations PASS')
if __name__=='__main__':main()

#!/usr/bin/env python3
import argparse,pathlib,subprocess,os,json,hashlib,shlex
ROOT=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=pathlib.Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
flags=['-std=c11','-O0','-g0','-pipe','-Wall','-Wextra','-Werror','-I'+str(ROOT/'sdk/app'),'-I'+str(ROOT/'sdk/driver')]
if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
source=ROOT/'test/rtc_utc/calendar_test.c';cc=os.environ.get('CC','cc');raw=subprocess.check_output([cc,*flags,'-MM',str(source)],text=True);paths=[pathlib.Path(x).resolve() for x in shlex.split(raw.replace('\\\n',' ').split(':',1)[1])]+[pathlib.Path(__file__).resolve()];before={str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in paths}
command=[cc,*flags,str(source),'-o',str(a.output/'test')];subprocess.run(command,check=True);result=subprocess.run([str(a.output/'test')],check=True,text=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});(a.output/'run.log').write_text(result.stdout+result.stderr);assert before=={str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in paths};(a.output/'receipt.json').write_text(json.dumps({'passed':True,'sanitized':a.sanitize,'command':command,'inputs':before,'binary_sha256':hashlib.sha256((a.output/'test').read_bytes()).hexdigest()},indent=2));print(result.stdout,end='')

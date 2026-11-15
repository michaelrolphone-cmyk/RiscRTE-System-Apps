#!/usr/bin/env python3
"""Canonical header provenance and actual timer clients; modeled callbacks only."""
import argparse,hashlib,json,os,pathlib,subprocess,tempfile
ROOT=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--runtime',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
header=ROOT/'lib/PortableApps/include/RiscRuntimeClockV1.h';expected='76584fdab224b184918d9f9cf675d5c4443adef7161e8518d456bc25277dc30f'
assert hashlib.sha256(header.read_bytes()).hexdigest()==expected
assert header.read_bytes()==(a.runtime/'sdk/app/RiscRuntimeClockV1.h').read_bytes()
common='''
static unsigned clock_calls,health_calls,diagnostics;
static bool clock_ok=true;
static bool counted_clock(uint32_t*out){++clock_calls;*out=123;return clock_ok;}
static bool counted_health(risc_runtime_health_v1*out){++health_calls;out->uptime_ms=456;return true;}
static bool diagnostic(const char*line){assert(strstr(line,"t_ms="));++diagnostics;return true;}
'''
rows=[]
for kind,base in [('context','runtime'),('broadcast','rt')]:
 src='#define main existing_main\n#include "'+str(ROOT/'test/native_apps'/f'{kind}_client_test.c')+'"\n#undef main\n#define PORTABLE_STAGE_LOGS\n#include "PortableStageLog.h"\n'+common
 client='contexts' if kind=='context' else 'broadcast';args='true,true' if kind=='context' else 'true'
 src+='''int main(void){
 risc_runtime_api_v1 selected=BASE;selected.health=counted_health;selected.monotonic_ms=counted_clock;selected.diagnostic=diagnostic;
 portable_CLIENT_client c;assert(portable_CLIENT_open(&c,&selected));
 assert(portable_CLIENT_step(&c,ARGS));assert(clock_calls==1&&!health_calls);
 portable_stage_log(&selected,"probe","");assert(clock_calls==2&&!health_calls&&diagnostics==1);
 clock_ok=false;unsigned before=steps;
 (void)portable_CLIENT_step(&c,ARGS);assert(clock_calls==3&&!health_calls&&steps==before);
 portable_stage_log(&selected,"probe","");assert(clock_calls==4&&!health_calls&&diagnostics==1);
 selected.monotonic_ms=NULL;assert(portable_CLIENT_step(&c,ARGS));assert(health_calls==1);
 selected.struct_size=offsetof(risc_runtime_api_v1,monotonic_ms);selected.monotonic_ms=counted_clock;
 assert(portable_CLIENT_step(&c,ARGS));assert(health_calls==2&&clock_calls==4);
 puts("Actual CLIENT timing and StageLog: suffix preferred, false authoritative, null/old-prefix fallback PASS");return 0;}
'''.replace('BASE',base).replace('CLIENT',client).replace('ARGS',args)
 for san in (False,True):
  with tempfile.TemporaryDirectory(prefix='clock-client-') as temp:
   t=pathlib.Path(temp);(t/'test.c').write_text(src)
   flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
   cmd=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-return-type',*flags,'-I'+str(ROOT/'lib/PortableApps/include'),str(t/'test.c'),'-o',str(t/'test')]
   subprocess.run(cmd,check=True);r=subprocess.run([t/'test'],check=True,capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});print(r.stdout,end='');rows.append({'client':kind,'sanitized':san,'stdout':r.stdout})
for san in (False,True):
 with tempfile.TemporaryDirectory(prefix='clock-catalog-') as temp:
  binary=pathlib.Path(temp)/'test';flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
  cmd=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-DTEST_MONOTONIC_CATALOG','-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),str(ROOT/'lib/PortableApps/src/PortableInstalledCatalog.c'),str(ROOT/'test/native_apps/installed_catalog_client_test.c'),'-o',str(binary)]
  subprocess.run(cmd,check=True);r=subprocess.run([binary],check=True,capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});print(r.stdout,end='');rows.append({'client':'installed-catalog','sanitized':san,'stdout':r.stdout})
(a.output/'evidence.json').write_text(json.dumps({'canonical_runtime':'37fca3e2244bf71ac64bd8cd7e62eb94f4c09a9d','canonical_header_sha256':expected,'runs':rows,'hardware_verified':False},indent=2)+'\n')

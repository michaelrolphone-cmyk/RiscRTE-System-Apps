#!/usr/bin/env python3
"""Negative/positive production boundary witnesses; baseline differs only at reset."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
from test_native_wifi import ROOT,stage,source_hashes

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,required=True);a=parser.parse_args()
    out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);hashes=source_hashes();results=[]
    for sanitized in (False,True):
        for baseline in (True,False):
            name=('sanitized' if sanitized else 'normal')+('-idle-only' if baseline else '-fixed')
            dest=out/name;dest.mkdir(exist_ok=True);stage(dest)
            if baseline:
                src=dest/'src';src.mkdir(exist_ok=True)
                for p in (ROOT/'lib/PortableApps/src').iterdir():
                    if p.is_file() and p.name!='wifi_native_view.inc' and not (src/p.name).exists():(src/p.name).symlink_to(p)
                p=ROOT/'lib/PortableApps/src/wifi_native_view.inc';s=p.read_text();old='wn_gesture=false;input_reset();';assert s.count(old)==1
                (src/p.name).write_text(s.replace(old,'wn_gesture=false;portable_touch_cancel_gesture(&touch);'))
                p=dest/'portable.c';p.write_text(p.read_text().replace(str(ROOT/'lib/PortableApps/src/adapter.c'),str(src/'adapter.c')))
            p=dest/'workflow.c';p.write_text(p.read_text().replace('++text_opens;', '++text_opens;fprintf(stderr,"EDITOR_OPEN delivered=%u\\n",ticks);'))
            p=dest/'portable.c';s=p.read_text();old='*e=raw_events[raw_read++];return 1;';assert old in s
            p.write_text(s.replace(old,'*e=raw_events[raw_read++];fprintf(stderr,"RAW kind=%u sequence=%llu captured=%llu delivered=%u\\n",e->kind,(unsigned long long)e->sequence,(unsigned long long)e->timestamp_ms,ticks);return 1;'))
            binary=dest/'test';flags=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
            cmd=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function',*flags,'-DTEST_WIFI_RGB_WIDTH=410','-DTEST_WIFI_RGB_HEIGHT=502','-DNATIVE_WIFI_WORKFLOW_FIXTURE="'+str(dest/'workflow.c')+'"','-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),str(ROOT/'test/native_apps/native_wifi_test.c'),'-o',str(binary)]
            subprocess.run(cmd,check=True)
            run=subprocess.run([str(binary),'3'],capture_output=True,text=True,timeout=45,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
            log=run.stdout+run.stderr;(dest/'run.log').write_text(log)
            if baseline:assert run.returncode<0 and 'EDITOR_OPEN' in log,log
            else:assert run.returncode==0,log
            results.append({'name':name,'command':cmd,'returncode':run.returncode,'expected_rejection':baseline,'log_sha256':hashlib.sha256(log.encode()).hexdigest()})
            binary.unlink();print(name+': expected '+('failure' if baseline else 'pass'),flush=True)
    if hashes!=source_hashes():raise RuntimeError('Production source changed during qualification')
    (out/'source-hashes.json').write_text(json.dumps(hashes,indent=2)+'\n')
    (out/'receipt.json').write_text(json.dumps({'tests':results,'baseline_change':'input_reset replaced by cancel_gesture at matching owned-frame completion only','hardware_tested':False},indent=2)+'\n')
if __name__=='__main__':main()

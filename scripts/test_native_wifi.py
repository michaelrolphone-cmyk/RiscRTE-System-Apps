#!/usr/bin/env python3
"""Qualify native Wi-Fi with production controller/adapter and monotonic providers."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def source_hashes():
    paths=[p for d in ('Apps','lib/PortableApps') for p in (ROOT/d).rglob('*') if p.is_file()]
    paths += [ROOT/'test/native_apps'/name for name in ('portable_wifi_test.c','wifi_workflow_test.c','native_wifi_test.c','native_wifi_keyboard_test.c')]
    paths.append(Path(__file__))
    return {str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(paths)}
def stage(out):
    s=(ROOT/'test/native_apps/portable_wifi_test.c').read_text()
    s=s.replace('"../../lib/PortableApps/src/adapter.c"','"'+str(ROOT/'lib/PortableApps/src/adapter.c')+'"').replace('"../../Apps/wifi_settings.c"','"'+str(ROOT/'Apps/wifi_settings.c')+'"')
    s=s.replace('static unsigned ticks,','static unsigned native_snapshots;static uint64_t native_clock_base;static bool native_zero_timestamps,native_sync_display,native_snapshot_fail;\nstatic unsigned ticks,')
    s=s.replace('h->uptime_ms=ticks;', 'h->uptime_ms=native_clock_base+ticks;')
    s=s.replace('.timestamp_ms=ticks,', '.timestamp_ms=native_zero_timestamps?0:native_clock_base+ticks,').replace('next.timestamp_ms=ticks;', 'next.timestamp_ms=native_zero_timestamps?0:native_clock_base+ticks;')
    s=s.replace('return poll_count<limit_polls;', 'return ticks<20000;')
    s=s.replace('script[i].at==poll_count && script[i].x>=0','ticks>=script[i].at && ticks<script[i].at+100 && script[i].x>=0')
    s=s.replace('if(script[i].at==poll_count)out->pressed=out->released=script[i].buttons;', 'if(script[i].buttons&&ticks>=script[i].at){out->pressed=out->released=script[i].buttons;script[i].buttons=0;}')
    s=s.replace('*out=raw_state;out->width=width();', '++native_snapshots;if(native_snapshot_fail)return false;*out=raw_state;out->width=width();')
    s=s.replace('static bool fake_present(', 'static unsigned native_present_wait,native_present_deadline;\nstatic bool fake_present(')
    s=s.replace('out->state=RISC_DISPLAY_PRESENT_COMPLETE;', 'if(!native_present_deadline)native_present_deadline=ticks+native_present_wait;out->state=ticks<native_present_deadline?RISC_DISPLAY_PRESENT_QUEUED:RISC_DISPLAY_PRESENT_COMPLETE;if(out->state==RISC_DISPLAY_PRESENT_COMPLETE)native_present_deadline=0;')
    s=s.replace('.supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565)};', '.supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565),.flags=native_sync_display?0:RISC_DISPLAY_INFO_ASYNC_PRESENT};')
    (out/'portable.c').write_text(s)
    s=(ROOT/'test/native_apps/wifi_workflow_test.c').read_text().replace('"portable_wifi_test.c"','"'+str(out/'portable.c')+'"')
    s=s.replace('if(!strcmp(n,RISC_TEXT_ENTRY_CAPABILITY)){assert(v==1&&!id);','if(!strcmp(n,RISC_TEXT_ENTRY_CAPABILITY)){if(!offered_text)return false;assert(v==1&&!id);')
    (out/'workflow.c').write_text(s)
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');p.add_argument('--size',default='');p.add_argument('--case',type=int);p.add_argument('--snapshot',action='store_true');p.add_argument('--captures',action='store_true');a=p.parse_args()
    out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);stage(out);rows=[];hashes=source_hashes()
    sizes=[tuple(map(int,a.size.split('x')))] if a.size else [(128,128),(160,128),(128,160),(160,160),(240,240),(410,502),(320,180),(502,240)]
    for w,h in sizes:
        binary=out/'test';flags=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie'] if a.sanitize else []
        if a.snapshot:flags+=['-DPORTABLE_RASTER_SNAPSHOT']
        cmd=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function',*flags,f'-DTEST_WIFI_RGB_WIDTH={w}',f'-DTEST_WIFI_RGB_HEIGHT={h}','-DNATIVE_WIFI_WORKFLOW_FIXTURE="'+str(out/'workflow.c')+'"', '-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),str(ROOT/'test/native_apps/native_wifi_test.c'),'-o',str(binary)]
        subprocess.run(cmd,check=True)
        for case in ([a.case] if a.case is not None else range(16)):
            env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
            if a.captures and case==11:
                captures=out/f'{w}x{h}-pages';captures.mkdir(exist_ok=True);env['PORTABLE_WIFI_FRAME_DIR']=str(captures)
            run=subprocess.run([str(binary),str(case)],capture_output=True,text=True,timeout=45,env=env)
            (out/f'{w}x{h}-{case}.log').write_text(run.stdout+run.stderr);print(run.stdout+run.stderr,end='',flush=True);run.check_returncode()
            rows.append({'width':w,'height':h,'case':case,'passed':True});env.pop('PORTABLE_WIFI_FRAME_DIR',None)
        binary.unlink()
        if a.case is None:
            local=cmd.copy();local[local.index(str(ROOT/'test/native_apps/native_wifi_test.c'))]=str(ROOT/'test/native_apps/native_wifi_keyboard_test.c')
            local += ['-DNATIVE_WIFI_LOCAL_FIXTURE="'+str(out/'portable.c')+'"']
            subprocess.run(local,check=True)
            for case in range(3):
                if a.captures and case==2:
                    captures=out/f'{w}x{h}-keyboard';captures.mkdir(exist_ok=True);env['PORTABLE_WIFI_FRAME_DIR']=str(captures)
                run=subprocess.run([str(binary),str(case)],capture_output=True,text=True,timeout=45,env=env)
                (out/f'{w}x{h}-keyboard-{case}.log').write_text(run.stdout+run.stderr);print(run.stdout+run.stderr,end='',flush=True);run.check_returncode()
                rows.append({'width':w,'height':h,'keyboard_case':case,'passed':True})
            binary.unlink();env.pop('PORTABLE_WIFI_FRAME_DIR',None)
    if hashes!=source_hashes():raise RuntimeError('Production source changed during qualification; rerun')
    (out/'source-hashes.json').write_text(json.dumps(hashes,indent=2)+'\n')
    (out/'receipt.json').write_text(json.dumps({'cases':rows,'sanitized':a.sanitize,'snapshot':a.snapshot,'scope':'Production controller, renderer and adapter. Synthetic providers; hardware not tested.'},indent=2)+'\n')
if __name__=='__main__':main()

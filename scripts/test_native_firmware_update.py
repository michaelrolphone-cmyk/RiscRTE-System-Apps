#!/usr/bin/env python3
"""Exercise production native Firmware Update with bounded, offline providers.

Only fixture providers are instrumented. Production controller, adapter, native
geometry, input, glyph and icon raster are compiled unchanged from this tree.
"""
import argparse
import hashlib
import json
import os
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PIN = '48229bd1593a2e24e95bf4537b495ee615990fd3'
SYSTEM_PIN = 'c94f367f6ca3677954b580833f222ae16775c964'
SIZES = ((240, 240), (410, 502), (160, 160), (320, 180), (502, 410))
CASE_NAMES = (
    'native-geometry-pagination-and-states', 'raw-touch-navigation-repeat-entry',
    'changed-confirmed-version', 'changed-visible-version', 'changed-before-begin',
    'verified-activation-after-radio-close', 'pending-frame-touch-rejected',
    'mode-change-cancels-gesture', 'back-drains-pending-frame',
    'message-redraw-preserves-gesture', 'footer-gutter-ignored',
    'suspend-clears-confirmation', 'suspend-refreshes-status',
    'close-before-begin-and-reopen', 'pending-frame-navigation-cannot-install',
    'external-exit-before-begin', 'release-change-during-frame',
    'removed-release-invalidates-confirmation', 'failed-cancel-retains-invocation',
    'failed-disconnect-retains-invocation', 'status-failure-cleanup',
    'service-error-keeps-current', 'unknown-activation-requires-restart',
    'idempotent-open-repeat-entry', 'native-glyph-icon-raster-and-all-hit-pixels',
    'changed-release-id', 'changed-installed-version', 'changed-release-reason',
    'changed-availability', 'changed-release-size', 'failed-selected-get',
    'unterminated-id', 'unterminated-version', 'unterminated-installed',
    'unterminated-reason', 'back-preempts-ready-activation',
    'raw-cancel-preempts-ready-activation', 'async-lease-ownership',
    'failed-present-status-retains', 'failed-present-state-retains',
    'superseded-present-retains', 'present-timeout-retains',
    'failed-submit-retains-frame', 'failed-radio-release-retry',
    'retained-http-keeps-radio', 'firmware-capability-denied-no-apps-fallback',
    'wrong-service-api-retains', 'short-service-table-retains',
    'missing-activation-callback-retains', 'status-change-invalidates-gesture',
    'usb-only-release-no-install', 'bounded-native-scene-captures',
    'catalog-unterminated-id', 'catalog-unterminated-version',
    'catalog-unterminated-installed', 'catalog-unterminated-reason',
)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def replace_once(src, before, after):
    if src.count(before) != 1:
        raise RuntimeError(f'Fixture seam must occur exactly once: {before!r}')
    return src.replace(before, after, 1)


def fixture_source():
    src = (ROOT / 'test/native_apps/portable_update_app_test.c').read_text()
    for path in ('lib/PortableApps/src/adapter.c', 'Apps/update_portable.inc'):
        src = replace_once(src, '"../../' + path + '"', json.dumps(str(ROOT / path)))
    src = replace_once(src, 'int main(int argc,char**argv)', 'static int original_main(int argc,char**argv)')
    src = replace_once(src, '.request_launch=fake_launch,', '.request_launch=fake_launch,.retain_invocation=native_retain,.exit_requested=native_exit,')
    src = replace_once(src, 'script[i].at==input_step() && script[i].x>=0', 'input_step()>=script[i].at&&input_step()<script[i].at+4&&script[i].x>=0')
    src = replace_once(src, 'static void observed(void){', 'static void observed(void){++native_callbacks;')
    src = replace_once(src, '.flags=UI_FLAGS}', '.flags=UI_FLAGS|RISC_DISPLAY_INFO_ASYNC_PRESENT}')
    src = replace_once(src, 'assert(!frame_count);frame_count=1;', 'assert(!frame_count&&!native_pending);++native_frame_acquires;frame_count=1;')
    src = replace_once(src, 'assert(f==1 && frame_count);frame_count=0;', 'assert(f==1 && frame_count&&!native_pending);++native_frame_releases;frame_count=0;')
    src = replace_once(src, '*t=++presents;', '*t=++presents;assert(!native_pending);native_pending=*t;native_present_deadline=ticks+native_present_delay;')
    src = replace_once(src, 'assert(token);out->state=RISC_DISPLAY_PRESENT_COMPLETE;', '''assert(token&&token==native_pending);++native_status_calls;
if(native_present_fault==1)return false;
out->state=native_present_fault==2?RISC_DISPLAY_PRESENT_FAILED:native_present_fault==3?RISC_DISPLAY_PRESENT_SUPERSEDED:(native_present_fault==4||ticks<native_present_deadline)?RISC_DISPLAY_PRESENT_QUEUED:RISC_DISPLAY_PRESENT_COMPLETE;
if(out->state==RISC_DISPLAY_PRESENT_COMPLETE)native_pending=0;''')
    src = replace_once(src, 'ticks+=ms;', 'ticks+=ms;native_tick();')
    src = replace_once(src, '#ifdef PORTABLE_WATCH_NATIVE_HEIGHT\n if(x>=0)', '#if defined(PORTABLE_WATCH_NATIVE_HEIGHT) && !defined(PORTABLE_UPDATE_NATIVE_UI)\n if(x>=0)')
    src = src.replace('239-out->contacts[0].x', 'UW-1-out->contacts[0].x').replace('239-out->contacts[0].y', 'UH-1-out->contacts[0].y')
    before = 'strcpy(out->id,"clock");strcpy(out->version,"1.1.0");strcpy(out->installed,"1.0.0");strcpy(out->reason,"Update available");out->availability=SOFTWARE_UPDATE_AVAILABLE;return true;'
    after = '''if(native_get_failure)return false;strcpy(out->id,native_release_id);strcpy(out->version,native_release_version);strcpy(out->installed,native_installed);strcpy(out->reason,native_reason);out->availability=native_availability;out->size=native_size;
if(native_malformed==1)memset(out->id,'X',sizeof(out->id));
if(native_malformed==2)memset(out->version,'X',sizeof(out->version));
if(native_malformed==3)memset(out->installed,'X',sizeof(out->installed));
if(native_malformed==4)memset(out->reason,'X',sizeof(out->reason));return true;'''
    src = replace_once(src, before, after)
    for name in ('refresh', 'step', 'status', 'get', 'begin', 'cancel', 'activate', 'restart'):
        start = src.index('static bool update_' + name + '(')
        pos = src.index('{', start) + 1
        src = src[:pos] + '++native_update_calls;' + src[pos:]
    src = replace_once(src, 'static const software_update_v1 update_api=', 'static software_update_v1 update_api=')
    src = replace_once(src, 'observed();assert(out->struct_size==sizeof(*out) && (v==1||v==2));', '''observed();
if(!strcmp(name,SOFTWARE_UPDATE_FIRMWARE_CAPABILITY)){++native_firmware_acquires;if(native_deny_firmware)return false;}
if(!strcmp(name,SOFTWARE_UPDATE_APPS_CAPABILITY))++native_apps_acquires;
assert(out->struct_size==sizeof(*out) && (v==1||v==2));''')
    return src


def source_paths():
    paths = [f for directory in ('Apps', 'lib/PortableApps', 'lib/NativeApps')
             for f in (ROOT / directory).rglob('*') if f.is_file() and f.suffix in ('.c', '.h', '.inc')]
    paths += [ROOT / 'test/native_apps/native_firmware_update_test.c',
              ROOT / 'test/native_apps/portable_update_app_test.c', Path(__file__).resolve()]
    return sorted(paths)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--snapshot', action='store_true')
    parser.add_argument('--size')
    parser.add_argument('--case', type=int, choices=range(len(CASE_NAMES)))
    parser.add_argument('--captures', action='store_true')
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    include = out / 'include'
    shutil.copytree(ROOT / 'lib/PortableApps/include', include, dirs_exist_ok=True)
    shutil.copytree(ROOT / 'lib/PortableApps/time', out / 'time', dirs_exist_ok=True)
    header = subprocess.check_output(['git', '-C', str(args.runtime), 'show', PIN + ':sdk/app/RiscRuntimeV1.h'])
    (include / 'RiscRuntimeV1.h').write_bytes(header)
    fixture = out / 'fixture.c'
    fixture.write_text(fixture_source())
    paths = source_paths()
    hashes = {str(f.relative_to(ROOT)): sha(f) for f in paths}
    rows = []
    captures = []
    sizes = [tuple(map(int, args.size.split('x')))] if args.size else SIZES
    commands = []
    for width, height in sizes:
        for rotation in (0, 180):
            binary = out / 'test'
            command = [os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                       '-Wno-unused-function', '-Wno-misleading-indentation', f'-DUW={width}', f'-DUH={height}',
                       f'-DPORTABLE_TOUCH_ROTATION={rotation}', '-DNATIVE_FIRMWARE_UPDATE_FIXTURE=' + json.dumps(str(fixture)),
                       '-I' + str(include), '-I' + str(ROOT / 'lib/NativeApps/include'),
                       str(ROOT / 'test/native_apps/native_firmware_update_test.c'), '-o', str(binary)]
            if args.snapshot:
                command.append('-DPORTABLE_RASTER_SNAPSHOT')
            if args.sanitize:
                command += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie']
            commands.append(command)
            subprocess.run(command, check=True)
            for case in ([args.case] if args.case is not None else range(len(CASE_NAMES))):
                env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0:abort_on_error=1', UBSAN_OPTIONS='halt_on_error=1')
                if args.captures and case == 51 and rotation == 0:
                    capture = out / f'{width}x{height}-rotation{rotation}'
                    capture.mkdir(exist_ok=True)
                    env['PORTABLE_UPDATE_FRAME_DIR'] = str(capture)
                result = subprocess.run([str(binary), str(case)], env=env, capture_output=True, text=True, timeout=30)
                (out / f'{width}x{height}-{rotation}-{case}.log').write_text(result.stdout + result.stderr)
                if result.returncode:
                    print(width, height, rotation, case, result.stdout, result.stderr, flush=True)
                result.check_returncode()
                rows.append({'width': width, 'height': height, 'rotation': rotation, 'case': case,
                             'name': CASE_NAMES[case], 'passed': True})
            print(f'{width}x{height} rotation{rotation}: passed', flush=True)
            binary.unlink()
    if hashes != {str(f.relative_to(ROOT)): sha(f) for f in paths}:
        raise RuntimeError('Source changed during qualification; rerun settled source')
    for path in sorted(out.glob('*-rotation*/*.ppm')):
        captures.append({'path': str(path.relative_to(out)), 'sha256': sha(path), 'bytes': path.stat().st_size})
    (out / 'source-hashes.json').write_text(json.dumps(hashes, indent=2) + '\n')
    (out / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
    receipt = {'status': 'PASS', 'cases': rows, 'sanitized': args.sanitize, 'snapshot': args.snapshot,
               'system_baseline_pin': SYSTEM_PIN, 'runtime_pin': PIN,
               'runtime_header_sha256': sha(include / 'RiscRuntimeV1.h'), 'fixture_sha256': sha(fixture),
               'captures': captures, 'hardware_tested': False, 'installed_or_published': False,
               'scope': 'Production Firmware Update native controller, adapter, glyph/icon raster, shared hit rectangles, raw input, separate firmware capability and checked custody with bounded synthetic providers.',
               'limits': 'Service and paired-bank/cohort implementation safety is covered by the separate existing service/cohort suites. Synthetic tests do not establish target hardware behavior. ASan leak detection is disabled because terminal retained invocations intentionally keep live resources.'}
    (out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Exercise first-visible-frame brightness on the full shared adapter recipe."""
from pathlib import Path
import argparse, json, os, shutil, subprocess, tempfile

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--source', type=Path, default=ROOT)
p.add_argument('--bridges', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--sanitize', action='store_true')
p.add_argument('--stage-profile', action='store_true', help='Select the final release diagnostic feature')
a = p.parse_args(); a.output.mkdir(parents=True, exist_ok=True)
results = []

def replace(source, old, new):
    assert source.count(old) == 1, old
    return source.replace(old, new)

with tempfile.TemporaryDirectory(prefix='initial-brightness-') as temporary:
    work = Path(temporary)
    for name in ('Apps', 'lib', 'test'):
        shutil.copytree(a.source / name, work / name)
    adapter = work / 'lib/PortableApps/src/adapter.c'
    with adapter.open('a') as f:
        f.write('''
/* Test-only accessors; no production branches or replacement implementations. */
void brightness_test_paint(void){clear_color(0x2468);present(false);}
bool brightness_test_progress(void){return canvas_progress();}
bool brightness_test_failed(void){return failed;}
bool brightness_test_preview(unsigned n){bool changed=false;quick.ui.brightness=n;quick.ui.action_brightness=n;return pqa_session_apply(&quick,rt,display,PQA_BRIGHTNESS_PREVIEW,&changed);}
bool brightness_test_torch(bool value){bool changed=false;quick.ui.torch=value;return pqa_session_apply(&quick,rt,display,PQA_TORCH,&changed);}
''')
    fixture = (ROOT / 'docs/t063/healthy-controller/probe24.c').read_text()
    fixture = replace(fixture, 'static unsigned fw,fh,pitch,ticks,polls,grants,subscriptions,frames,presents,launches;',
        'static unsigned fw,fh,pitch,ticks,polls,grants,subscriptions,frames,presents,launches;\n'
        'static unsigned fixture_completed,fixture_brightness,fixture_brightness_calls,fixture_fault,fixture_status_reads;\n'
        'static bool fixture_retained,fixture_user_brightness,fixture_has_brightness;')
    fixture = replace(fixture, '.flags=display_flags|(use_async?',
        '.flags=(fixture_has_brightness?RISC_DISPLAY_INFO_BRIGHTNESS:0)|display_flags|(use_async?')
    fixture = replace(fixture, 'assert(f==1&&frames);frames=0;*token=++presents;guards();',
        'assert(f==1&&frames);if(fixture_fault==1)return false;frames=0;*token=++presents;fixture_status_reads=0;guards();')
    fixture = replace(fixture,
        'static bool nt_brightness(void *c,uint16_t value,uint16_t maximum) {(void)c;assert(value<=maximum);return true;}',
        'static bool nt_brightness(void *c,uint16_t value,uint16_t maximum) {(void)c;assert(value<=maximum);assert(fixture_user_brightness||(!frames&&fixture_completed));assert(!fixture_retained);fixture_brightness_calls++;if(fixture_fault==3)return false;fixture_brightness=value;return true;}')
    fixture = replace(fixture,
        '(void)c;assert(token);out->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;',
        '(void)c;assert(token);if(fixture_fault==2){out->state=RISC_DISPLAY_PRESENT_FAILED;return true;}if(++fixture_status_reads<3){out->state=RISC_DISPLAY_PRESENT_ACTIVE;return true;}fixture_completed++;out->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;')
    fixture = replace(fixture,
        'static bool nt_retain(void) {assert(!"healthy fixture cannot retain");return false;}',
        'static bool nt_retain(void) {fixture_retained=true;return true;}')
    fixture_path = work / 'brightness-fixture.c'; fixture_path.write_text(fixture)
    paths = dict(system=str(work), fixture=str(ROOT / 'docs/t063/healthy-controller'),
                 bridge=str(a.bridges.resolve()), output=str(a.output.resolve()))
    command = [x.format_map(paths) for x in json.loads((ROOT / 'docs/t063/healthy-controller/command24.json').read_text())]
    command = [str(ROOT / 'test/native_apps/initial_brightness_checks.inc') if x == paths['fixture'] + '/probe24.c' else x for x in command]
    command.insert(1, '-x'); command.insert(2, 'c')
    command += ['-DINITIAL_BRIGHTNESS_FIXTURE="' + str(fixture_path) + '"',
                '-DSPRINGBOARD_SOURCE="' + str(work / 'Apps/springboard.c') + '"']
    if a.stage_profile:
        command += ['-DPORTABLE_STAGE_PROFILE']
    if a.sanitize:
        command += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie']
    (a.output / 'command.json').write_text(json.dumps(command, indent=2) + '\n')
    subprocess.run(command, check=True)
    for width, height in ((240, 240), (410, 502)):
        for mode in ('sync', 'async'):
            for scenario in ('dark-open', 'retained-lit', 'preview-before', 'torch-before',
                             'unsupported', 'submit-failed', 'completion-failed', 'brightness-failed'):
                run = subprocess.run([str(a.output.resolve() / 'probe24'), str(width), str(height), mode, scenario],
                    env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'}, capture_output=True, text=True, timeout=30)
                name = f'{width}x{height}-{mode}-{scenario}'
                (a.output / (name + '.log')).write_text(run.stdout + run.stderr)
                if run.returncode:
                    print(run.stdout, run.stderr); run.check_returncode()
                result = {'case': name, 'result': run.stdout.splitlines()[-1]}
                results.append(result); print(result['result'], flush=True)
    (a.output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')

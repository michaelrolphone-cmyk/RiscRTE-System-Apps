#!/usr/bin/env python3
"""Run the preserved Springboard controller through dark/retained re-entry.

The independent RGB source checkout supplies the production code and its
qualified first-frame fixture. Only a temporary copy of that fixture changes.
"""
import argparse
import json
import shutil
import subprocess
import tempfile
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--springboard-root', type=Path, required=True)
p.add_argument('--bridges', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--sanitize', action='store_true')
a = p.parse_args()
source = a.springboard_root.resolve()
output = a.output.resolve()
output.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix='springboard-launch-brightness-') as td:
    root = Path(td)
    files = ['scripts/test_initial_frame_brightness.py',
             'test/native_apps/initial_brightness_checks.inc',
             'docs/t063/healthy-controller/probe24.c',
             'docs/t063/healthy-controller/catalog24.c',
             'docs/t063/healthy-controller/command24.json']
    for name in files:
        dest = root / name
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source / name, dest)
    checks = root / files[1]
    text = checks.read_text()
    anchor = '  unsigned before=fixture_brightness_calls;\n'
    assert text.count(anchor) == 1
    case = '''  if(!strncmp(scenario,"application-",12)) {
    full_run=5;contact_count=1;
    fixture_brightness=!strcmp(scenario,"application-retained")?40:0;
    for(unsigned invocation=0;invocation<3;++invocation) {
      if(invocation) {
        polls=0;fixture_completed=0;
        fixture_brightness=!strcmp(scenario,"application-retained")?40:0;
        assert(app_module_init()==0);
      }
      unsigned before=fixture_brightness_calls;
      app_main();settle_brightness();
      assert(!brightness_test_failed() && fixture_completed);
      assert(fixture_brightness==40 && fixture_brightness_calls==before+1);
      unsigned colored=0;
      for(unsigned y=0;y<fh;y++)for(unsigned x=0;x<fw;x++)
        colored+=pixels[y*pitch+x]!=0;
      assert(colored>100 && !launches);
      app_module_fini();assert(!grants&&!frames&&!subscriptions);
    }
    free(guarded);assert(!allocation_count);
    printf("Application brightness %ux%u %s %s: real controller, visible pixels, three clean invocations PASS\\n",fw,fh,argv[3],scenario);
    return 0;
  }
'''
    checks.write_text(text.replace(anchor, case + anchor))
    script = root / files[0]
    text = script.read_text()
    anchor = "'unsupported', 'submit-failed', 'completion-failed', 'brightness-failed'):"
    assert text.count(anchor) == 1
    text = text.replace(anchor, "'unsupported', 'submit-failed', 'completion-failed', 'brightness-failed',\n                             'application-dark', 'application-retained'):")
    script.write_text(text)
    command = ['python3', str(script), '--source', str(source), '--bridges', str(a.bridges.resolve()),
               '--output', str(output), '--stage-profile']
    if a.sanitize:
        command.append('--sanitize')
    subprocess.run(command, check=True)
    (output / 'source.json').write_text(json.dumps({
        'source_commit': subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip(),
        'hardware_tested': False,
        'production_source_changed': False,
        'real_controller_cases': 8,
        'invocations_per_controller_case': 3,
    }, indent=2) + '\n')

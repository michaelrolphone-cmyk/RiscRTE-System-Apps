#!/usr/bin/env python3
"""Build the production alarm adapter, verify raw native input and source closure."""
import argparse, hashlib, json, os, re, shlex, struct, subprocess, zlib
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument('--output', type=Path, required=True)
p.add_argument('--normal-only', action='store_true')
p.add_argument('--profile', choices=('native', 'native-height', 'off'))
p.add_argument('--quick', type=int, choices=(0, 1))
p.add_argument('--rotation', type=int, choices=(0, 180))
p.add_argument('--snapshot', type=int, choices=(0, 1))
a = p.parse_args()
out = a.output.resolve(); out.mkdir(parents=True, exist_ok=True)
s = (ROOT / 'test/native_apps/portable_handoff_test.c').read_text()
replacements = {
 '#include "../../lib/PortableApps/src/adapter.c"': '#include ' + json.dumps(str(ROOT / 'lib/PortableApps/src/adapter.c')),
 'mock_pixels[240*243]': '*mock_pixels', 'sizeof(mock_pixels)': 'geometry_size',
 '.width=240,.height=240': '.width=geometry_width,.height=geometry_height',
 '.stride_bytes=486,': '.stride_bytes=geometry_stride,',
 'mock_owned[4]': 'mock_owned[128]', 'i<4': 'i<128',
 '.acquire=mock_acquire,.release=mock_release': '.acquire=ana_acquire,.release=mock_release',
 '.present_status=mock_status};': '.present_status=mock_status,.set_brightness=ana_brightness};',
 'mock_poll_touch,mock_next,mock_snapshot': 'ana_poll,ana_next,mock_snapshot',
}
for before, after in replacements.items():
 assert before in s, before
 s = s.replace(before, after)
fixture = out / 'fixture.c'; fixture.write_text(s)
rows = []; dependencies = {}; pixel_oracles = {}
profiles = (a.profile,) if a.profile else ('native', 'native-height', 'off')
quicks = (a.quick,) if a.quick is not None else (0, 1)
snapshots = (a.snapshot,) if a.snapshot is not None else (0, 1)
compiler = os.environ.get('CC', 'cc')
compiler_version = subprocess.check_output([compiler, '--version'], text=True).splitlines()[0]
def save():
 paths = ['test/native_apps/alarm_native_adapter_test.c', 'test/native_apps/portable_handoff_test.c', 'scripts/test_alarm_native_adapter.py']
 result = {'rows': rows, 'hardware': False, 'compiler': compiler_version,
  'source_sha256': {n: hashlib.sha256((ROOT/n).read_bytes()).hexdigest() for n in paths},
  'compiler_dependency_sha256': dependencies,
  'dependency_scope': 'All compiler -MM non-system dependencies for every built configuration. Generated fixture derives from the recorded source fixture and runner.',
  'pixel_comparison': 'Every common native/legacy render key must match across Quick selections, direct/snapshot, rotation, and normal/sanitized rows.'}
 (out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
# Complete bounded normal coverage before compiling any sanitizer row.
for sanitized in ((False,) if a.normal_only else (False, True)):
 for profile in profiles:
  for quick in quicks:
   rotations = (a.rotation,) if a.rotation is not None else ((0, 180) if profile != 'off' else (0,))
   for rotation in rotations:
    for snapshot in snapshots:
     name = f'{profile}-quick{quick}-rot{rotation}-snapshot{snapshot}-san{int(sanitized)}'
     binary = out / 'test-binary'
     flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie'] if sanitized else []
     selection = ['-DPORTABLE_NATIVE_CANVAS', '-DPORTABLE_ALARM_NATIVE'] if profile != 'off' else []
     if profile == 'native-height': selection += ['-DPORTABLE_WATCH_NATIVE_HEIGHT']
     if quick: selection += ['-DPORTABLE_QUICK_ACTIONS'] + (['-DPORTABLE_QUICK_NATIVE'] if profile != 'off' else [])
     if snapshot: selection += ['-DPORTABLE_RASTER_SNAPSHOT']
     cmd = [compiler, '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', *flags, *selection,
      '-DPORTABLE_NOVA_UI', '-DPORTABLE_ALARM_CLIENT', '-DPORTABLE_RETAINED_RGB565_HANDOFF',
      f'-DPORTABLE_TOUCH_ROTATION={rotation}', '-DFRAME_FIXTURE=' + json.dumps(str(fixture)),
      '-I' + str(ROOT/'lib/PortableApps/include'), '-I' + str(ROOT/'lib/NativeApps/include'),
      str(ROOT/'test/native_apps/alarm_native_adapter_test.c')]
     if quick: cmd += [str(ROOT/'lib/PortableApps/src'/n) for n in ('quick_actions.c', 'quick_render.c', 'quick_session.c')]
     dep = subprocess.run([cmd[0], '-MM', *cmd[1:]], text=True, capture_output=True, check=True)
     (out/(name+'.d')).write_text(dep.stdout)
     selected = []
     for token in shlex.split(dep.stdout.replace('\\\n', ' ')):
      if token.endswith(':'): continue
      path = Path(token).resolve()
      key = 'generated:fixture.c' if path == fixture else str(path.relative_to(ROOT))
      digest = hashlib.sha256(path.read_bytes()).hexdigest()
      assert key not in dependencies or dependencies[key] == digest, ('source changed during matrix', key)
      dependencies[key] = digest; selected.append(key)
     try:
      built = subprocess.run([*cmd, '-o', str(binary)], text=True, capture_output=True)
      (out/(name+'-build.log')).write_text(built.stdout+built.stderr)
      if built.returncode: print(built.stdout+built.stderr, flush=True)
      built.check_returncode()
      run_env = {**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0', 'UBSAN_OPTIONS': 'halt_on_error=1'}
      if not sanitized and profile == 'native' and not quick and not rotation:
       capture_dir = out / ('captures-snapshot' + str(snapshot)); capture_dir.mkdir(exist_ok=True)
       run_env['ANA_CAPTURE_DIR'] = str(capture_dir)
      result = subprocess.run([str(binary)], text=True, capture_output=True, timeout=240, env=run_env)
      (out/(name+'.log')).write_text(result.stdout+result.stderr)
      print(name, result.stdout+result.stderr, flush=True); result.check_returncode()
      render = dict(re.findall(r'^RENDER (.+) ([0-9a-f]{16})$', result.stdout, re.M))
      assert render, name
      for key, value in render.items():
       oracle_key = ('off' if profile == 'off' else 'native', key)
       assert oracle_key not in pixel_oracles or pixel_oracles[oracle_key] == value, (name, key, value, pixel_oracles.get(oracle_key))
       pixel_oracles[oracle_key] = value
      rows.append({'name': name, 'profile': profile, 'quick': bool(quick), 'rotation': rotation, 'snapshot': bool(snapshot),
       'sanitized': sanitized, 'passed': True, 'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
       'renders': render, 'compiler_dependencies': sorted(set(selected))})
      save()
     finally:
      if binary.exists(): binary.unlink()
# Lossless PNG copies of actual provider pixels, with no image-library dependency.
for ppm in out.glob('captures-*/*.ppm'):
 header, dimensions, maximum, pixels = ppm.read_bytes().split(b'\n', 3)
 assert header == b'P6' and maximum == b'255'
 width, height = map(int, dimensions.split()); assert len(pixels) == width*height*3
 def chunk(tag, data):
  return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag+data) & 0xffffffff)
 scanlines = b''.join(b'\0' + pixels[y*width*3:(y+1)*width*3] for y in range(height))
 png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0))
 png += chunk(b'IDAT', zlib.compress(scanlines)) + chunk(b'IEND', b'')
 ppm.with_suffix('.png').write_bytes(png)
print(f'PASS {len(rows)} adapter configurations; render hashes agree across equivalent rows.', flush=True)

#!/usr/bin/env python3
"""Keep historical scene provider SDKs usable without the new pointer suffix."""
from pathlib import Path
import json,shutil,tempfile
from scene_sdk import stage_sdk
ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='scene-sdk-') as folder:
 root=Path(folder);system=root/'system';runtime=root/'runtime';out=root/'out'
 for directory in ('sdk','lib/PortableApps/include'):
  shutil.copytree(ROOT/directory,system/directory)
 for name in ('Services/scene_profile/SceneProfileV1.h','Services/text_input/SceneKeyboardV1.h'):
  target=system/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(ROOT/name,target)
 include=stage_sdk(runtime,system,out)
 assert (include/'RiscScenePointerV1.h').read_bytes()==(ROOT/'sdk/app/RiscScenePointerV1.h').read_bytes()
 (system/'sdk/app/RiscScenePointerV1.h').unlink()
 include=stage_sdk(runtime,system,out)
 assert not (include/'RiscScenePointerV1.h').exists()
 assert 'RiscScenePointerV1.h' not in json.loads((include/'.scene-sdk-files.json').read_text())
 assert (include/'RiscSceneHoldV1.h').is_file()
 (system/'sdk/app/RiscSceneHoldV1.h').unlink()
 try:stage_sdk(runtime,system,root/'missing-core')
 except FileNotFoundError:pass
 else:raise AssertionError('Missing established contract was silently ignored')
print('Scene SDK: optional pointer extension, stale removal, required core verified')

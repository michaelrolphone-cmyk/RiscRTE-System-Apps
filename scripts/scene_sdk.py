"""Stage one canonical SDK per translation unit; never mix pragma-once copies."""
from pathlib import Path
import json
import shutil


def stage_sdk(runtime: Path, system: Path, output: Path) -> Path:
    include = output / 'include'
    include.mkdir(parents=True, exist_ok=True)
    sources = {}

    def canonical(source):
        previous = sources.get(source.name)
        if previous is not None and previous.read_bytes() != source.read_bytes():
            raise ValueError(f'Conflicting canonical SDK header: {source.name}')
        sources[source.name] = source

    for part in ('app', 'driver', 'hardware'):
        for source in (runtime / 'sdk' / part).glob('*.h'):
            canonical(source)
    for name in ('RiscDisplayOutputV1.h', 'RiscTouchV1.h', 'RiscInputNavigationV1.h'):
        canonical(system / 'lib/PortableApps/include' / name)
    # Optional provider contracts come from System when not in this Runtime.
    # Test this invocation's sources, not files left by an earlier build: stale
    # headers otherwise silently hide a newly appended provider API suffix.
    for name in ('RiscRuntimeHostControlV1.h', 'RiscSceneV1.h', 'RiscSceneStateV1.h', 'RiscSceneLifecycleV1.h',
                 'RiscSceneComponentsV1.h', 'RiscScenePageV1.h',
                 'RiscSceneCheckpointV1.h', 'RiscSceneReadingV1.h', 'RiscSceneReadingLinksV1.h',
                 'RiscCivilClockV1.h', 'RiscSceneResidentV1.h'):
        if name not in sources:
            canonical(system / ('lib/PortableApps/include' if name=='RiscRuntimeHostControlV1.h' else 'sdk/app') / name)
    canonical(system / 'Services/scene_profile/SceneProfileV1.h')
    canonical(system / 'Services/text_input/SceneKeyboardV1.h')
    manifest = include / '.scene-sdk-files.json'
    previous = json.loads(manifest.read_text()) if manifest.exists() else []
    for name in previous:
        if name not in sources and Path(name).name == name and name.endswith('.h'):
            (include / name).unlink(missing_ok=True)
    for name, source in sources.items():
        destination = include / name
        if source.resolve() == destination.resolve():
            raise ValueError('SDK output aliases a source header')
        shutil.copyfile(source, destination)
    manifest.write_text(json.dumps(sorted(sources)) + '\n')
    return include

"""Stage the transitive shared-Home header closure into one canonical directory."""
import hashlib,re,shutil
from pathlib import Path

QUOTED_INCLUDE=re.compile(r'^\s*#\s*include\s*"([^"]+)"',re.M)
def stage_home_headers(root, target, roots=None):
    root,target=Path(root),Path(target)
    sdk=root/'sdk/app';portable=root/'lib/PortableApps/include'
    seeds=[*sdk.glob('RiscCanvas*.h'),*(sdk/n for n in (
        'RiscHomePresentationV1.h','RiscResidentContextsV1.h','RiscCivilClockV1.h')),
        *portable.glob('PortableHome*.h')]
    copied={}
    def copy(source):
        destination=target/source.name
        shutil.copyfile(source,destination)
        copied[source.relative_to(root).as_posix()]=hashlib.sha256(destination.read_bytes()).hexdigest()
        return destination
    if roots is None:
        pending=[copy(source) for source in seeds]
    else:
        pending=[]
        for name in roots:
            if Path(name).name!=name:raise ValueError('Invalid shared Home root '+name)
            destination=target/name
            if destination.is_file():pending.append(destination);continue
            source=next((base/name for base in (sdk,portable) if (base/name).is_file()),None)
            if source is None:raise ValueError('Missing shared Home root '+name)
            pending.append(copy(source))
    seen=set()
    while pending:
        header=pending.pop().resolve()
        if header in seen:continue
        seen.add(header)
        for name in QUOTED_INCLUDE.findall(header.read_text()):
            candidate=header.parent/name
            if not candidate.is_file():
                candidate=target/name
            if not candidate.is_file():
                # Only missing owner-published dependencies are added. Never
                # replace the already selected Runtime/physical prefix SDK.
                source=next((base/name for base in (sdk,portable) if (base/name).is_file()),None)
                if source is None:
                    raise ValueError('Missing shared Home header '+name+' required by '+str(header))
                if Path(name).name!=name:
                    raise ValueError('Unstaged relative shared Home dependency '+name)
                candidate=copy(source)
            pending.append(candidate)
    return {'copied_source_sha256':copied,
            'closure_sha256':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(seen)}}

#!/usr/bin/env python3
"""Exact-source, software-only mouse consumer qualification."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def git(repo, *args):
    return subprocess.check_output(["git", "-C", str(repo), *args], text=True).strip()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--runtime", type=Path, required=True)
    p.add_argument("--reader", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--sanitize", action="store_true")
    args = p.parse_args()
    args.runtime, args.reader, args.output = args.runtime.resolve(), args.reader.resolve(), args.output.resolve()
    provenance = json.loads((ROOT / "lib/PortableApps/MOUSE_CONSUMER_PROVENANCE.json").read_text())
    for repo, commit in ((args.runtime, provenance["runtime_commit"]), (args.reader, provenance["reader_commit"])):
        assert git(repo, "rev-parse", "HEAD") == commit, repo
        assert not git(repo, "status", "--porcelain"), f"Upstream must be clean: {repo}"
    for f in provenance["files"]:
        origin = args.reader if f["repository"] == "T5S3-Reader" else args.runtime
        data = subprocess.check_output(["git", "-C", str(origin), "show", f'{f["commit"]}:{f["source"]}'])
        assert data == (ROOT / f["destination"]).read_bytes() == (origin / f["source"]).read_bytes(), f
        assert sha(ROOT / f["destination"]) == f["sha256"], f
    # Reject modified tracked source; untracked output/input directories are not code.
    assert not git(ROOT, "status", "--porcelain", "--untracked-files=no"), "System tracked source must be clean"
    args.output.mkdir(parents=True, exist_ok=True)
    commands = []
    log_path = args.output / "tests.log"
    source = ROOT / "lib/PortableApps/src/PortableMouseConsumer.c"
    fixture = ROOT / "test/native_apps/portable_mouse_consumer_test.c"
    with log_path.open("w") as log:
        def run(command, **kwargs):
            command = list(map(str, command)); commands.append(command)
            log.write("$ " + " ".join(command) + "\n"); log.flush()
            subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True, **kwargs)
        cc = os.environ.get("CC", "cc")
        run([cc, "--version"])
        san = ["-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-fno-omit-frame-pointer"] if args.sanitize else []
        with tempfile.TemporaryDirectory(prefix="mouse-consumer-") as directory:
            build = Path(directory)
            includes = build / "sdk"
            shutil.copytree(ROOT / "lib/PortableApps/include", includes)
            # Existing System prefix and full current native Runtime both compile.
            for label in ("system-prefix", "native-sdk"):
                if label == "native-sdk":
                    shutil.copy(args.runtime / "sdk/app/RiscRuntimeV1.h", includes)
                common = ["-g", "-O1", "-Wall", "-Wextra", "-Werror", "-I" + str(includes)]
                run([cc, "-std=c11", *common, *san, "-no-pie", "-DPORTABLE_USB_MOUSE", source, fixture, "-o", build / label])
                run([build / label])
            disabled = build / "disabled.c"
            disabled.write_text('#include "PortableMouseConsumer.h"\n#include <assert.h>\n'
                                'int main(void){portable_mouse_consumer c;assert(!portable_mouse_init(&c,0,0,0));'
                                'assert(c.result==PMC_UNSUPPORTED && !c.initialized && !c.cleanup_required);return 0;}\n')
            run([cc, "-std=c11", *common, *san, "-no-pie", source, disabled, "-o", build / "disabled"])
            run([build / "disabled"])
            cpp = build / "headers.cpp"
            cpp.write_text('#include "PortableMouseConsumer.h"\n'
                           'static_assert(sizeof(risc_usb_mouse_state_v1)==24,"mouse state");\n'
                           'static_assert(sizeof(risc_usb_mouse_event_v1)==64,"mouse event");\n'
                           'int main(){return 0;}\n')
            run([os.environ.get("CXX", "c++"), "-std=c++17", *common, cpp, "-o", build / "headers"])
            run([build / "headers"])
        env = dict(os.environ, MOUSE_RUNTIME_SOURCE=str(args.runtime), MOUSE_READER_SOURCE=str(args.reader),
                   SANITIZE="1" if args.sanitize else "0")
        run(["bash", ROOT / "scripts/run_mouse_runtime_integration.sh"], env=env)
    inputs = [p for pattern in ("lib/PortableApps/*MOUSE*", "lib/PortableApps/include/PortableMouseConsumer.h",
                               "lib/PortableApps/src/PortableMouseConsumer.c", "test/native_apps/*mouse*",
                               "scripts/*mouse*", "docs/PORTABLE_MOUSE_CONSUMER.md") for p in ROOT.glob(pattern) if p.is_file()]
    evidence = dict(result="PASS", sanitized=args.sanitize, system_source=git(ROOT, "rev-parse", "HEAD"),
                    system_status=git(ROOT, "status", "--porcelain").splitlines(),
                    source_locks=provenance, source_hashes={str(p.relative_to(ROOT)): sha(p) for p in inputs},
                    runtime_tree=git(args.runtime, "rev-parse", "HEAD^{tree}"),
                    reader_tree=git(args.reader, "rev-parse", "HEAD^{tree}"),
                    commands=commands, log_sha256=sha(log_path),
                    sanitizer_environment={k: os.environ.get(k, "") for k in ("ASAN_OPTIONS", "UBSAN_OPTIONS", "LSAN_OPTIONS")},
                    coverage=["system-prefix and complete pinned-native SDK strict C", "C++ ABI and opt-out",
                              "copied-event, focus, GAP, snapshot, disconnect, malformed/retained unit paths",
                              "maximum capacity, malformed invocation tags, rejected retention fences",
                              "8 actual Runtime/provider eager/demand cases including missing-platform refusal"],
                    ci={k: os.environ.get(k, "") for k in ("GITHUB_SHA", "GITHUB_RUN_ID", "GITHUB_RUN_ATTEMPT")},
                    not_run=["target build/timing", "safe production poll executor", "product UI routing",
                             "physical USB/X4 electrical acceptance", "LeakSanitizer", "target executable mapping retention"])
    (args.output / "evidence.json").write_text(json.dumps(evidence, indent=2) + "\n")
    print(f"Mouse consumer {'ASan/UBSan' if args.sanitize else 'normal'} PASS: {log_path}")


if __name__ == "__main__":
    main()

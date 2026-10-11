#!/usr/bin/env python3
"""Run production Files acquisition/controller against real bootstrap Runtime.

The target record is mandatory: its storage and handler defines, manifest and
required grants select the fixture. Only the relevant storage/file.open policy
is projected into this host test; this is not full display/toolbar or hardware
qualification. UI drawing is inert, while Runtime, graph, module lifecycle,
file.open broker, controller acquisition/loading/cleanup and dispatch are real.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
RUNTIME_COMMIT = "c546dae32e2e75f7e7f4867dc788b6be53ded64c"


def run(command, **kwargs):
    subprocess.run(list(map(str, command)), check=True, **kwargs)


def selected_build(build):
    record = json.loads((build / "file_browser-build-record.json").read_text())
    manifest = json.loads((build / "file_browser.json").read_text())
    elf = (build / "file_browser.elf").read_bytes()
    assert hashlib.sha256(elf).hexdigest() == record["sha256"], "target ELF does not match selected record"
    assert len(elf) == record["size_bytes"], "target ELF size does not match selected record"
    for source in ("Apps/file_browser.c", "Apps/file_browser_portable.inc",
                   "Apps/file_browser_operations.inc", "Apps/file_browser_paper.inc"):
        assert hashlib.sha256((ROOT / source).read_bytes()).hexdigest() == record["source_sha256"][source], "controller source changed after selected target build: " + source
    assert manifest["requires"] == record["requested_capabilities"], "target manifest/record requirements disagree"
    prefixes = ("-DPORTABLE_FILE_BROWSER_", "-DFILE_BROWSER_RETURN_APP=")
    defines = [value for value in record["defines"] if value.startswith(prefixes)]
    assert defines.count("-DPORTABLE_FILE_BROWSER_APP") == 1
    assert defines.count("-DPORTABLE_FILE_BROWSER_HANDLERS") == 1, "target must enable real file.open dispatch"
    assert not any("SECONDARY_INSTANCE" in value for value in defines), "fixture covers one X4 SD volume"
    for prefix in ("-DPORTABLE_FILE_BROWSER_CAPABILITY=", "-DPORTABLE_FILE_BROWSER_INSTANCE="):
        assert sum(value.startswith(prefix) for value in defines) == 1, "target storage define missing or duplicate"
    storage = [entry for entry in manifest["requires"] if entry["capability"] in ("storage.volume", "storage.installed-files")]
    assert len(storage) == 1, "target must declare exactly one storage capability"
    requires = storage + [entry for entry in manifest["requires"] if entry["capability"] == "file.open"]
    assert len(requires) == 2
    grants = []
    for need in requires:
        matches = [grant for grant in record["required_grants"] if all(grant.get(key) == value for key, value in need.items())]
        assert len(matches) == 1, "target requirement must have exactly one selected grant"
        grants.extend(matches)
    # The X4 fixture's actual board has one SD volume at instance 9. No rewrite
    # of an installed-files identity into a volume identity is allowed here.
    assert requires == [{"capability": "storage.volume", "api": 1}, {"capability": "file.open", "api": 1}], "selected target must request storage.volume and file.open"
    assert grants == [{"capability": "storage.volume", "api": 1, "instance_id": 9}, {"capability": "file.open", "api": 1, "instance_id": 0}], "selected target grants must be volume9/file.open0"
    return record, defines, {"requires": requires, "grants": grants}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True, help="selected native Files target output directory")
    parser.add_argument("--runtime", type=Path, required=True, help="read-only Runtime checkout at the pinned commit")
    args = parser.parse_args()
    build, runtime = args.build.resolve(), args.runtime.resolve()
    assert subprocess.check_output(["git", "-C", str(runtime), "rev-parse", "HEAD"], text=True).strip() == RUNTIME_COMMIT
    assert not subprocess.check_output(["git", "-C", str(runtime), "status", "--porcelain", "--untracked-files=no"], text=True).strip(), "Runtime tracked sources must match the pinned commit"
    record, defines, selection = selected_build(build)
    include = build / "native-time-sdk/include"
    assert (include / "RiscRuntimeV1.h").is_file(), "selected target's staged native SDK is required"
    output = build / "runtime-fixture"
    output.mkdir(exist_ok=True)
    selection_path = output / "selection.json"
    selection_path.write_text(json.dumps(selection, indent=2) + "\n")
    sources = [runtime / value for value in (
        "src/bootstrap/Json.cpp", "src/bootstrap/Board.cpp", "src/bootstrap/Runtime.cpp",
        "src/runtime/drivers/ProviderGraphV2.cpp", "src/runtime/drivers/ProviderModuleV2.cpp")]
    host_includes = [runtime / value for value in (
        "src", "sdk/app", "sdk/driver", "sdk/hardware", "lib/ArduinoJson/src", "test/drivers/stubs")]
    fixture_includes = [include, ROOT / "lib/NativeApps/include", runtime / "sdk/driver", runtime / "sdk/hardware"]
    fixtures = ROOT / "test/native_apps/fixtures"
    for sanitized in (False, True):
        directory = output / ("sanitized" if sanitized else "normal")
        directory.mkdir(exist_ok=True)
        sanitizer = ["-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-fno-omit-frame-pointer"] if sanitized else []
        # Keep Runtime's canonical host-test optimization level (unoptimized).
        # Controller/provider fixtures still use O1 in both instrumented runs.
        common = ["-g", "-Wall", "-Wextra", "-Werror", *sanitizer]
        cflags = [os.environ.get("CC", "cc"), "-std=c11", "-O1", *common, "-fPIC", "-fvisibility=hidden", "-shared",
                  *["-I" + str(path) for path in fixture_includes]]
        for name, mutation in (
            ("selected", []),
            ("wrong-instance", ["-UPORTABLE_FILE_BROWSER_INSTANCE", "-DPORTABLE_FILE_BROWSER_INSTANCE=8u"]),
            ("wrong-capability", ["-UPORTABLE_FILE_BROWSER_CAPABILITY", '-DPORTABLE_FILE_BROWSER_CAPABILITY="storage.installed-files"']),
        ):
            run([*cflags, *defines, *mutation, fixtures / "file_browser_runtime_app.c", "-o", directory / f"browser-{name}.elf"])
        run([*cflags, fixtures / "file_browser_runtime_provider.c", "-o", directory / "volume.elf"])
        run([*cflags, fixtures / "file_browser_runtime_receiver.c", "-o", directory / "receiver.elf"])
        executable = directory / "test"
        run([os.environ.get("CXX", "c++"), "-std=c++17", *common, "-Wno-missing-field-initializers", "-rdynamic", "-no-pie",
             *["-I" + str(path) for path in host_includes], *sources,
             ROOT / "test/native_apps/file_browser_runtime_test.cpp", "-ldl", "-o", executable])
        run([executable, directory, selection_path], timeout=60)
    receipt = {"runtime_commit": RUNTIME_COMMIT, "target_sha256": record["sha256"], "target_defines": defines,
               "policy_projection": selection, "normal": "passed", "address_undefined_sanitizers": "passed",
               "asan_options": os.environ.get("ASAN_OPTIONS", ""),
               "scope": "production acquisition/controller and bootstrap Runtime; host media/UI fixtures, no hardware qualification"}
    receipt["source_sha256"] = {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                               for path in [Path(__file__).resolve(), ROOT / "test/native_apps/file_browser_runtime_test.cpp",
                                            *sorted(fixtures.glob("file_browser_runtime_*.c"))]}
    (output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print("Files production Runtime acquisition/controller: normal + ASan/UBSan PASS")


if __name__ == "__main__":
    main()

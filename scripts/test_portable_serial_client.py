#!/usr/bin/env python3
"""Pinned, software-only portable serial client tests and Runtime integration."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
RUNTIME = "6a3b3d46f5b2857eacc6e9863799dec1f466b036"
HEADERS = {
    "app": ["RiscRuntimeV1.h", "RiscStreamClientV1.h", "RiscSerialStreamSessionV1.h"],
    "driver": ["RiscStreamResultV1.h", "RiscStreamSessionProviderV1.h", "RiscProviderV2.h", "RiscStreamProviderV1.h"],
}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=ROOT / "build/portable-serial-client")
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    args.runtime = args.runtime.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    assert subprocess.check_output(["git", "-C", str(args.runtime), "rev-parse", "HEAD"], text=True).strip() == RUNTIME
    assert not subprocess.check_output(["git", "-C", str(args.runtime), "status", "--porcelain"], text=True).strip(), "Runtime checkout must be clean"
    provenance = json.loads((ROOT / "lib/PortableApps/SERIAL_CLIENT_PROVENANCE.json").read_text())
    for entry in provenance["files"]:
        assert sha(ROOT / entry["destination"]) == entry["sha256"], entry
    inputs = ["lib/PortableApps/include/PortableSerialClient.h", "lib/PortableApps/src/PortableSerialClient.c",
              "test/native_apps/portable_serial_client_test.c", "scripts/test_portable_serial_client.py",
              "lib/PortableApps/SERIAL_CLIENT_PROVENANCE.json", "docs/PORTABLE_SERIAL_CLIENT.md"]
    commands = []
    log_path = args.output / "tests.log"
    with log_path.open("w") as log:
        def run(command, **kwargs):
            command = list(map(str, command))
            commands.append(command)
            log.write("$ " + " ".join(command) + "\n")
            log.flush()
            subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True, **kwargs)

        with tempfile.TemporaryDirectory(prefix="portable-serial-") as directory:
            build = Path(directory)
            includes = build / "sdk"
            shutil.copytree(ROOT / "lib/PortableApps/include", includes)
            canonical = {}
            for group, names in HEADERS.items():
                for name in names:
                    path = f"sdk/{group}/{name}"
                    data = subprocess.check_output(["git", "-C", str(args.runtime), "show", f"{RUNTIME}:{path}"])
                    assert data == (args.runtime / path).read_bytes(), path
                    canonical[path] = hashlib.sha256(data).hexdigest()
                    (includes / name).write_bytes(data)
            common = ["-O1", "-g", "-Wall", "-Wextra", "-Werror", "-I" + str(includes),
                      "-I" + str(ROOT / "lib/NativeApps/include")]
            sanitizer = ["-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-fno-omit-frame-pointer"] if args.sanitize else []
            cc = os.environ.get("CC", "cc")
            run([cc, "-std=c11", *common, *sanitizer, "-DPORTABLE_SERIAL_STREAMS",
                 ROOT / inputs[2], ROOT / inputs[1], "-o", build / "unit"])
            run([build / "unit"])
            disabled = build / "disabled.c"
            disabled.write_text('#include "PortableSerialClient.h"\n#include <assert.h>\n'
                                'int main(void){portable_serial_client c;assert(!portable_serial_init(&c,0));'
                                'assert(c.result==PSC_UNSUPPORTED && !c.initialized && !c.cleanup_required);return 0;}\n')
            run([cc, "-std=c11", *common, *sanitizer, disabled, ROOT / inputs[1], "-o", build / "disabled"])
            run([build / "disabled"])
            cpp = build / "headers.cpp"
            cpp.write_text('#include "PortableSerialClient.h"\n'
                           'static_assert(sizeof(risc_serial_stream_open_v1)==32,"open layout");\n'
                           'static_assert(sizeof(risc_serial_stream_call_v1)==24,"call layout");\n'
                           'static_assert(sizeof(t5_serial_config_t)==8,"config layout");\n'
                           'int main(){return 0;}\n')
            run([os.environ.get("CXX", "c++"), "-std=c++17", *common, cpp, "-o", build / "headers"])
            run([build / "headers"])
        env = dict(os.environ, SERIAL_SYSTEM_SOURCE=str(ROOT), SANITIZE="1" if args.sanitize else "0")
        run(["bash", args.runtime / "test/run_serial_stream_client_integration_test.sh"], env=env)
    evidence = {
        "result": "PASS", "sanitized": args.sanitize,
        "sanitizer_environment": {name: os.environ.get(name, "") for name in ("ASAN_OPTIONS", "UBSAN_OPTIONS", "LSAN_OPTIONS")},
        "system_source": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
        "system_status": subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT, text=True).splitlines(),
        "runtime_source": RUNTIME, "runtime_headers": canonical,
        "source_hashes": {p: sha(ROOT / p) for p in inputs}, "imported_headers": provenance["files"],
        "commands": commands, "log_sha256": sha(log_path),
        "coverage": ["invalid arguments and prefix bounds", "explicit instance/index and complete inventory",
                     "unknown inventory recovery", "partial RX/TX and accepted-byte saturation",
                     "cancel/stall/wrap", "detach/generation change", "stale copied object/invocation",
                     "retained open/call/read/write/close/release", "malformed counts/status/output",
                     "1..1000 ms allocated total controls including broker open rollback",
                     "opt-out and C++ header smoke", "22 production Runtime integration cases"],
        "not_run": ["physical USB/serial/radio I/O", "hardware qualification", "monitor UI", "product pin/publication"],
    }
    (args.output / "evidence.json").write_text(json.dumps(evidence, indent=2) + "\n")
    print(f"Portable serial client {'ASan/UBSan' if args.sanitize else 'normal'} PASS: {log_path}")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Build and validate the development TCP listener facade; no deployment."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
NATIVE_SHA256 = "b748d83d6d6e3f143a36b26993b6b9495ddd11e14eeb083325c1e0ff4563fcc5"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=ROOT / "dist/portable/tcp-listener")
    args = parser.parse_args()
    cc = os.environ.get("NATIVE_APP_CC") or shutil.which("xtensa-esp32s3-elf-gcc")
    if not cc:
        cc = str(Path(os.environ.get("PLATFORMIO_CORE_DIR", Path.home() / ".platformio")) /
                 "packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc")
    if not Path(cc).is_file():
        parser.error("ESP32-S3 toolchain unavailable; set NATIVE_APP_CC or PLATFORMIO_CORE_DIR")
    cxx, nm = cc.removesuffix("gcc") + "g++", cc.removesuffix("gcc") + "nm"
    source = ROOT / "Services/tcp_listener/service.cpp"
    native_header = source.parent / "native/RiscTcpListenerV1.h"
    if hashlib.sha256(native_header.read_bytes()).hexdigest() != NATIVE_SHA256:
        raise ValueError("Native TCP ABI differs from the pinned runtime contract")
    manifest = json.loads((source.parent / "manifest.json").read_text())
    if manifest["requires"] != [{"capability": "platform.tcp-listener", "api": 1}]:
        raise ValueError("TCP facade must require only its explicit native TCP dependency")
    if manifest["provides"] != [{"capability": "network.tcp.listener", "api": 1}]:
        raise ValueError("TCP facade capability mismatch")
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    exports = output / "exports.map"
    exports.write_text("{ global: t5_driver_get; local: *; };\n")
    common = ["-std=c++17", "-Os", "-fPIC", "-mtext-section-literals", "-mlongcalls",
              "-fvisibility=hidden", "-ffreestanding", "-fno-builtin", "-fno-exceptions", "-fno-rtti",
              "-fno-unwind-tables", "-fno-asynchronous-unwind-tables", "-nostdlib", "-nostartfiles",
              "-Wall", "-Wextra", "-Werror", "-I" + str(ROOT / "lib/PortableApps/include")]
    elf = output / manifest["file_name"]
    subprocess.run([cxx, *common, "-shared", "-Wl,--hash-style=sysv",
                    "-Wl,--version-script=" + str(exports), str(source), "-o", str(elf)], check=True)
    symbols = subprocess.check_output([nm, "-D", str(elf)], text=True)
    imports = {line.split()[-1] for line in symbols.splitlines() if " U " in " " + line}
    if not imports <= {"memcpy", "memset", "strcmp"}:
        raise ValueError("Unexpected imports: " + str(imports))
    actual_exports = {line.split()[-1] for line in symbols.splitlines()
                      if len(line.split()) >= 3 and line.split()[-2] in ("T", "D", "B", "R")}
    if actual_exports != {"t5_driver_get"}:
        raise ValueError("Unexpected exports: " + str(actual_exports))
    validator = output / "validate-elf"
    subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-I" + str(ROOT / "test/native_apps/stubs"),
                    "-I" + str(ROOT / "lib/elf_loader/include"),
                    str(ROOT / "lib/elf_loader/src/esp_elf_validate.c"),
                    str(ROOT / "test/native_apps/validate_test.c"), "-o", str(validator)], check=True)
    subprocess.run([str(validator), str(elf)], check=True)
    data = elf.read_bytes()
    shoff = struct.unpack_from("<I", data, 32)[0]
    shsize, shnum, shstr = struct.unpack_from("<HHH", data, 46)
    sections = [struct.unpack_from("<10I", data, shoff + i * shsize) for i in range(shnum)]
    names = data[sections[shstr][4]:sections[shstr][4] + sections[shstr][5]]
    sizes = {names[s[0]:].split(b"\0", 1)[0].decode(): s[5] for s in sections}
    if not 2048 <= sizes.get(".bss", 0) <= 4096:
        raise ValueError("Provider bounded workspace exceeded")
    if any(sizes.get(section, 0) for section in (".init_array", ".ctors")):
        raise ValueError("Provider must not require global constructors")
    probe = output / "stack-probe.o"
    subprocess.run([cxx, *common, "-fstack-usage", "-c", str(source), "-o", str(probe)], check=True)
    stack_frames = {}
    for line in probe.with_suffix(".su").read_text().splitlines():
        function, size, kind = line.split("\t")
        stack_frames[function] = int(size)
        if kind != "static" or int(size) > 512:
            raise ValueError("Provider stack budget exceeded: " + line)
    sources = [source, native_header, source.parent / "manifest.json", Path(__file__),
               ROOT / "lib/PortableApps/include/RiscTcpConnectionV1.h",
               ROOT / "lib/PortableApps/include/RiscProviderV2.h",
               ROOT / "lib/PortableApps/include/RiscStreamProviderV1.h",
               ROOT / "scripts/test_tcp_listener_service.sh",
               ROOT / "test/native_apps/tcp_listener_service_test.cpp"]
    record = {
        "purpose": "development-only-not-activation-or-publication",
        "runtime_contract_revision": "96e8c1f4e76ba3accfdff33d81b06233ecc5e7f5",
        "runtime_contract_sha256": NATIVE_SHA256,
        "sha256": hashlib.sha256(data).hexdigest(), "size_bytes": len(data),
        "compiler": subprocess.check_output([cxx, "--version"], text=True).splitlines()[0],
        "imports": sorted(imports), "exports": sorted(actual_exports),
        "section_sizes": sizes, "stack_frames": stack_frames,
        "source_sha256": {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                          for path in sources},
        "qualification_scope": "Host fake-native lifecycle and target ELF structure; no device/network test",
    }
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    (output / "build-record.json").write_text(json.dumps(record, indent=2) + "\n")
    print(f"TCP listener target ELF validated: {elf} ({len(data)} bytes)")


if __name__ == "__main__":
    main()

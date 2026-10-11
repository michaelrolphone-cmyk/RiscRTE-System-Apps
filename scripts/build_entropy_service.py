#!/usr/bin/env python3
"""Build and validate the development entropy facade; no deployment."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
NATIVE_SHA256 = "08b4d52e71b1349a57d1713e54bcc7872ae63d55edf647ec17b0bbb2f509c0d1"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=ROOT / "dist/portable/entropy")
    args = parser.parse_args()
    cc = os.environ.get("NATIVE_APP_CC") or shutil.which("xtensa-esp32s3-elf-gcc")
    if not cc:
        cc = str(Path(os.environ.get("PLATFORMIO_CORE_DIR", Path.home() / ".platformio")) /
                 "packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc")
    if not Path(cc).is_file():
        parser.error("ESP32-S3 toolchain unavailable; set NATIVE_APP_CC or PLATFORMIO_CORE_DIR")
    cxx, nm = cc.removesuffix("gcc") + "g++", cc.removesuffix("gcc") + "nm"
    source = ROOT / "Services/entropy/service.cpp"
    native_header = source.parent / "native/RiscEntropyV1.h"
    if hashlib.sha256(native_header.read_bytes()).hexdigest() != NATIVE_SHA256:
        raise ValueError("Native entropy ABI differs from the pinned runtime contract")
    manifest = json.loads((source.parent / "manifest.json").read_text())
    if manifest["requires"] != [{"capability": "platform.entropy", "api": 1}]:
        raise ValueError("Entropy facade must require only its explicit native entropy dependency")
    if manifest["provides"] != [{"capability": "crypto.entropy", "api": 1}]:
        raise ValueError("Entropy facade capability mismatch")
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
    if not 32 <= sizes.get(".bss", 0) <= 256:
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
               ROOT / "lib/PortableApps/include/RiscEntropySourceV1.h",
               ROOT / "lib/PortableApps/include/RiscProviderV2.h",
               ROOT / "lib/PortableApps/include/RiscStreamProviderV1.h",
               ROOT / "scripts/test_entropy_service.sh",
               ROOT / "test/native_apps/entropy_service_test.cpp"]
    record = {
        "purpose": "development-only-not-activation-or-publication",
        "runtime_contract_revision": "67e0dbe1a5f519fd96d8f88797ff91b9e1a3096c",
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
    print(f"Entropy target ELF validated: {elf} ({len(data)} bytes)")


if __name__ == "__main__":
    main()

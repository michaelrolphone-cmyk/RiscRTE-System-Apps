#!/usr/bin/env python3
"""Qualify the exact native glyph scan converter; never infer hardware FPS."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "test/native_apps/springboard_vector_review_test.c"
VECTOR = ROOT / "lib/PortableApps/src/nova_vector.inc"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def target_frames(elf, objdump, output):
    disassembly = subprocess.check_output([objdump, "-d", str(elf)], text=True)
    (output / "target-disassembly.txt").write_text(disassembly)
    frames = {}
    pattern = r"^[0-9a-f]+ <([^>]+)>:\n[^\n]*\bentry\s+a1, ([0-9xa-f]+)"
    for name, size in re.findall(pattern, disassembly, re.MULTILINE):
        if any(part in name for part in ("np_vector_", "nova_render", "nova_run", "app_main", "np_icon")):
            frames[name] = int(size, 0)
    curves = [size for name, size in frames.items()
              if name in ("np_vector_quad", "np_vector_cubic")]
    if len(curves) != 2:
        raise ValueError("Target debug ELF must expose both live curve functions")
    return {"status": "checked", "elf_sha256": digest(elf), "frames_bytes": frames,
            "max_curve_frame_bytes": max(curves),
            "scope": "Linked Xtensa prologues; not total task stack high-water"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "build/t030-native-vectors")
    parser.add_argument("--target-debug-elf", type=Path)
    parser.add_argument("--target-objdump", help="Xtensa objdump, required with target debug ELF")
    args = parser.parse_args()
    if bool(args.target_debug_elf) != bool(args.target_objdump):
        parser.error("--target-debug-elf and --target-objdump must be supplied together")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    # Both recursive functions must retain an explicit finite depth check.
    limits = [int(n) for n in re.findall(r"if\(depth>=(\d+)\s*\|\|", VECTOR.read_text())]
    if len(limits) != 2 or len(set(limits)) != 1:
        raise ValueError("Expected matching explicit quadratic/cubic recursion bounds")
    cc = shlex.split(os.environ.get("CC", "cc"))
    results = []
    for sanitized in (False, True):
        variant = "sanitized" if sanitized else "normal"
        binary = output / variant
        flags = (["-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                  "-fno-omit-frame-pointer", "-no-pie"] if sanitized else ["-fstack-usage"])
        command = [*cc, "-std=c11", "-O1" if sanitized else "-O2",
                   "-Wall", "-Wextra", "-Werror", *flags, str(SOURCE), "-o", str(binary)]
        (output / (variant + "-command.json")).write_text(json.dumps(command, indent=2) + "\n")
        subprocess.run(command, check=True, cwd=output)
        run = subprocess.run([str(binary)], text=True, capture_output=True, timeout=120,
                             env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"})
        (output / (variant + ".log")).write_text(run.stdout + run.stderr)
        print(run.stdout, end="")
        run.check_returncode()
        records = [json.loads(line[len("VECTOR_REVIEW "):])
                   for line in run.stdout.splitlines() if line.startswith("VECTOR_REVIEW ")]
        if len(records) != 1:
            raise ValueError("Expected one complete vector qualification record")
        results.append({"variant": variant, **records[0]})
    if {k: v for k, v in results[0].items() if k != "variant"} != {
            k: v for k, v in results[1].items() if k != "variant"}:
        raise ValueError("Sanitized and normal qualification results differ")
    host_frames = {}
    for path in output.glob("normal*.su"):
        for line in path.read_text().splitlines():
            name, size, kind = line.split("\t")
            if "np_vector_" in name:
                host_frames[name.rsplit(":", 1)[-1]] = {"bytes": int(size), "kind": kind}
    target = (target_frames(args.target_debug_elf.resolve(), args.target_objdump, output)
              if args.target_debug_elf else {"status": "not_requested"})
    if target["status"] == "checked":
        target["max_live_curve_frame_bytes"] = target["max_curve_frame_bytes"] * (limits[0] + 1)
    inputs = [SOURCE, VECTOR, ROOT / "lib/PortableApps/fonts/native_vectors.inc",
              ROOT / "lib/PortableApps/fonts/native_vectors_sources.json", Path(__file__).resolve()]
    result = {"schema": 1, "scope": "Exact embedded corpus, analytic winding and malformed bounds",
              "checks": results, "recursion_depth_limit": limits[0], "max_live_curve_frames": limits[0] + 1,
              "host_stack_frames": host_frames, "target_stack": target,
              "compiler": subprocess.check_output([*cc, "--version"], text=True).splitlines()[0],
              "source_sha256": {str(p.relative_to(ROOT)): digest(p) for p in inputs}}
    (output / "results.json").write_text(json.dumps(result, indent=2) + "\n")
    print("PASS native vector qualification: normal and ASan/UBSan; " + str(output / "results.json"))


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Read immutable Git blobs to inventory storage evidence, never migration closure.

python scripts/audit_storage_refs.py --manifest sources.json \
    --checkout-root /path/to/repositories --output report.json
Checkouts must already contain every manifest commit. No network or checkout
mutation is performed by this script (Git may hydrate a partial clone).
Evidence records exact line numbers and immutable links, not copied source text.
Replacement objects are disabled for every Git invocation.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
from urllib.parse import quote


PATTERNS = {
    "physical_api": r"\b(?:SPIFFS|LittleFS|SD_MMC|SD|SdFat|sdmmc_card_t|nvs_open|esp_vfs_spiffs_register)\b",
    "physical_path": r"/(?:sd|spiffs|sdcard|usb)(?:/|[\"'])",
    "legacy_file_contract": r"(?:storage\.(?:volume|installed-files|user-files|app-data\.export)|RiscStorageVolume|T5StorageApi|T5FileBrowserApi)",
    "generic_file_contract": r"(?:storage\.files|RiscStorageFiles)",
    "storage_contract": r"\bstorage\.[A-Za-z0-9_.-]+",
    "persistence_contract": r"(?:storage\.(?:app-data|key-value)|RiscAppData|RiscKeyValue|\bfopen\s*\()",
    "block_transport_contract": r"(?:usb\.device\.msc|RiscStorageExport|RiscUsbDeviceMsc)",
    "file_io": r"\b(?:fread|fwrite|fclose|opendir|readdir|closedir|fsync|fflush|fseek|ftell|unlink|rename|mkdir|rmdir)\s*\(",
    "indirect_storage_api": r"(?:->|\.)\s*(?:file_|dir_|storage_|sd_|usb_|read_file|write_file|remove_file|rename_file|installed_apps|app_data|key_value)|\b(?:T5AppApi|T5AppRuntime|T5InstalledApps|RiscInstalledApps|RiscFileOpen|T5FileOpen)",
}
REGEX = {key: re.compile(value) for key, value in PATTERNS.items()}
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inc", ".ino", ".ipp", ".rs", ".s"}
CONFIG_SUFFIXES = {".json", ".yaml", ".yml", ".toml", ".ini", ".cfg", ".cmake", ".mk", ".ld"}
TOOL_SUFFIXES = {".py", ".sh", ".bash", ".js", ".ts", ".mjs", ".ps1"}
MAX_BLOB_BYTES = 2 * 1024 * 1024


def git(root, *args, input_bytes=None):
    return subprocess.check_output(["git", "--no-replace-objects", "-C", str(root), *args], input=input_bytes)


def role_for(path):
    """Conservative path hints, not an inference that a match is a defect."""
    p = PurePosixPath(path)
    parts = {part.lower() for part in p.parts}
    if parts & {"archive", "archives", "historical", "legacy", "custody", "reference", "snapshots", "replay"}:
        return "historical"
    if parts & {"test", "tests", "fixtures", "fixture"} or p.name.startswith("test_"):
        return "test_fixture"
    if parts & {"docs", "doc", "evidence", "receipts"}:
        return "documentation_evidence"
    if parts & {"generated", "assets", "fonts", "font", "images", "dist", "build", "release", "releases"} or any(x.startswith("build-") for x in parts):
        return "generated_or_asset"
    if parts & {"vendor", "third_party", "third-party", "node_modules"}:
        return "third_party"
    if p.suffix.lower() in TOOL_SUFFIXES or "scripts" in parts or "tools" in parts:
        return "tooling"
    if p.suffix.lower() in CONFIG_SUFFIXES or p.name in {"CMakeLists.txt", "Makefile", "Kconfig"}:
        return "configuration"
    if parts & {"providers", "drivers", "services"}:
        return "provider_or_service"
    if "apps" in parts:
        return "application"
    if parts & {"sdk", "include"} and p.suffix.lower() in {".h", ".hpp", ".hh", ".hxx"}:
        return "interface_or_header"
    return "shared_or_runtime"


def source_candidate(path):
    p = PurePosixPath(path)
    return p.suffix.lower() in SOURCE_SUFFIXES | CONFIG_SUFFIXES | TOOL_SUFFIXES or p.name in {"CMakeLists.txt", "Makefile", "Kconfig", ".gitmodules"}


def tree_entries(root, commit):
    for row in git(root, "ls-tree", "-rz", commit).split(b"\0"):
        if row:
            metadata, path = row.split(b"\t", 1)
            mode, kind, blob = metadata.decode("ascii").split()
            yield mode, kind, blob, path.decode("utf-8", "strict")


def read_blobs(root, ids):
    """Batch object reads preserve exact bytes and avoid worktree/symlink reads."""
    if not ids:
        return {}, {}
    unique = list(dict.fromkeys(ids))
    raw = git(root, "cat-file", "--batch-check", input_bytes=("\n".join(unique) + "\n").encode())
    sizes = {}
    for expected, row in zip(unique, raw.decode("ascii").splitlines(), strict=True):
        parts = row.split()
        if len(parts) != 3 or parts[0] != expected or parts[1] != "blob":
            raise ValueError(f"Unavailable blob {expected}: {row}")
        sizes[expected] = int(parts[2])
    selected = [blob for blob in unique if sizes[blob] <= MAX_BLOB_BYTES]
    if not selected:
        return {}, sizes
    raw = git(root, "cat-file", "--batch", input_bytes=("\n".join(selected) + "\n").encode())
    blobs, offset = {}, 0
    for expected in selected:
        end = raw.index(b"\n", offset)
        actual, kind, size = raw[offset:end].decode("ascii").split()
        size = int(size)
        if actual != expected or kind != "blob" or size != sizes[expected]:
            raise ValueError(f"Unexpected object response for {expected}")
        start = end + 1
        blobs[actual] = raw[start:start + size]
        offset = start + size
        if raw[offset:offset + 1] != b"\n":
            raise ValueError(f"Truncated object response for {expected}")
        offset += 1
    if offset != len(raw):
        raise ValueError("Trailing bytes in object response")
    return blobs, sizes


def scan_repository(source, root):
    name, commit = source["repository"], source["commit"]
    if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", name):
        raise ValueError("Expected owner/repository")
    if not re.fullmatch(r"[0-9a-f]{40}(?:[0-9a-f]{24})?", commit):
        raise ValueError("A full immutable commit hash is required, not a branch or tag")
    actual = git(root, "rev-parse", "--verify", commit + "^{commit}").decode().strip()
    if actual != commit:
        raise ValueError("Resolved object is not the requested commit")
    entries = list(tree_entries(root, commit))
    candidates, skipped, unresolved = [], Counter(), []
    for mode, kind, blob, path in entries:
        if kind == "commit":
            skipped["gitlink_not_descended"] += 1
            unresolved.append({"path": path, "reason": "gitlink_not_descended", "commit": blob})
        elif mode == "120000":
            skipped["symlink_not_followed"] += 1
            unresolved.append({"path": path, "reason": "symlink_not_followed", "blob": blob})
        elif kind != "blob":
            raise ValueError(f"Unexpected tree object {kind}: {path}")
        elif not source_candidate(path):
            skipped["non_source_extension"] += 1
        else:
            candidates.append((blob, path))
    blobs, sizes = read_blobs(root, [blob for blob, _ in candidates])
    files, scanned = [], Counter()
    for blob, path in candidates:
        if sizes[blob] > MAX_BLOB_BYTES:
            skipped["oversize_blob"] += 1
            unresolved.append({"path": path, "reason": "oversize_blob", "bytes": sizes[blob], "blob": blob})
            continue
        data = blobs[blob]
        if b"\0" in data:
            skipped["binary_blob"] += 1
            unresolved.append({"path": path, "reason": "binary_blob", "blob": blob})
            continue
        try:
            lines = data.decode("utf-8", "strict").splitlines()
        except UnicodeDecodeError:
            skipped["non_utf8_blob"] += 1
            unresolved.append({"path": path, "reason": "non_utf8_blob", "blob": blob})
            continue
        role = role_for(path)
        scanned[role] += 1
        hits = []
        for number, line in enumerate(lines, 1):
            kinds = [key for key, regex in REGEX.items() if regex.search(line)]
            if kinds:
                hits.append({"line": number, "kinds": kinds})
        if hits:
            files.append({"path": path, "blob": blob, "sha256": hashlib.sha256(data).hexdigest(), "bytes": len(data), "role_hint": role,
                          "url": f"https://github.com/{name}/blob/{commit}/{quote(path, safe='/')}", "hits": hits})
    return {"repository": name, "commit": commit, "default_branch": source.get("default_branch"), "ref_kind": source.get("ref_kind", "default_branch"), "source_ref": source.get("source_ref"), "source_mode": "immutable Git blobs; worktree and index ignored",
            "tracked_entries": len(entries), "scanned_files": sum(scanned.values()), "scanned_by_role": dict(sorted(scanned.items())),
            "skipped_by_reason": dict(sorted(skipped.items())), "unresolved_paths": unresolved,
            "matching_files": len(files), "files": files}


def make_report(manifest, checkout_root):
    seen, entries = set(), []
    for source in manifest["repositories"]:
        name = source["repository"]
        if name in seen:
            raise ValueError(f"Duplicate repository in source manifest: {name}")
        seen.add(name)
        if source.get("status") == "empty_no_refs":
            entries.append({"repository": name, "status": "empty_no_refs", "files": [], "matching_files": 0, "evidence": source["evidence"]})
            continue
        entries.append(scan_repository(source, Path(checkout_root) / name.rsplit("/", 1)[-1]))
    return {"schema": 2, "source_manifest_sha256": hashlib.sha256(json.dumps(manifest, sort_keys=True, separators=(",", ":")).encode()).hexdigest(),
            "scope": "All RiscRTE applications remain required; this manifest bounds evidence only, not migration scope.",
            "limitations": ["Static lexical discovery, not proof of migration, semantic compatibility or absence of transitive storage use.",
                            "Only manifest commits are scanned; other branches, release tags, binaries and live devices are not qualified.",
                            "Role hints are path heuristics. Providers, headers, tests, configuration and historical hits are not automatically consumer defects.",
                            "Non-source extensions, symlinks, gitlinks, binary/non-UTF-8 and oversize blobs are explicitly outside this scan."],
            "patterns": PATTERNS, "max_blob_bytes": MAX_BLOB_BYTES, "source_suffixes": sorted(SOURCE_SUFFIXES | CONFIG_SUFFIXES | TOOL_SUFFIXES), "repositories": entries}


def write_split_report(report, directory):
    """Bounded per-repository JSON plus hashes for reproducible verification."""
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    index = {key: value for key, value in report.items() if key != "repositories"}
    index["repositories"] = []
    for entry in report["repositories"]:
        filename = entry["repository"].replace("/", "--") + ".json"
        data = (json.dumps(entry, separators=(",", ":")) + "\n").encode()
        (directory / filename).write_bytes(data)
        summary = {key: value for key, value in entry.items() if key not in {"files", "unresolved_paths"}}
        summary.update({"report_file": filename, "report_sha256": hashlib.sha256(data).hexdigest(),
                        "unresolved_paths": entry.get("unresolved_paths", [])})
        index["repositories"].append(summary)
    (directory / "index.json").write_text(json.dumps(index, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--checkout-root", type=Path, required=True)
    output = parser.add_mutually_exclusive_group(required=True)
    output.add_argument("--output", type=Path)
    output.add_argument("--output-dir", type=Path)
    args = parser.parse_args()
    report = make_report(json.loads(args.manifest.read_text()), args.checkout_root)
    if args.output_dir:
        write_split_report(report, args.output_dir)
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps([{key: value for key, value in entry.items() if key not in {"files", "unresolved_paths"}} for entry in report["repositories"]], indent=2))


if __name__ == "__main__":
    main()

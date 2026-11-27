#!/usr/bin/env python3
"""Independently qualify Firmware Update assembly and reject targeted faults.

No network, publication, device access, or production-source changes occur.
The supplied real assembly is decoded and compared against an independently
derived allowed delta. Negative assembler cases call its actual main() with
faults at the decoded-store boundary and suppress repeated output packing.
"""
import argparse
import contextlib
import copy
import hashlib
import io
import json
import sys
import zipfile
from pathlib import Path, PurePosixPath
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
LANES = ("watch", "wave", "x4")
SELECTION = {"watch": "twatch", "wave": "waveshare", "x4": "x4"}
CATALOG = {"capability": "app.catalog", "api": 1, "instance_id": 0}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def require(ok, message):
    if not ok:
        raise ValueError(message)


def file_tree(root):
    return {str(p.relative_to(root)): p.read_bytes()
            for p in root.rglob("*") if p.is_file()}


def zip_tree(path):
    with zipfile.ZipFile(path) as archive:
        names = [n for n in archive.namelist() if not n.endswith("/")]
        require(len(names) == len(set(names)), "Duplicate SD archive entry")
        for name in names:
            p = PurePosixPath(name)
            require(not p.is_absolute() and ".." not in p.parts,
                    "Unsafe SD archive entry")
        return {name: archive.read(name) for name in names}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("application", "providers", "bootfs-tools", "assembly",
                "downloads", "output"):
        parser.add_argument("--" + key, type=Path, required=True)
    args = parser.parse_args()
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=False)
    sys.path.insert(0, str(args.bootfs_tools.resolve()))
    import read_only_spiffs
    import assemble_firmware_scene_delivery as assembler
    from compose_shared_app_cohort import encode
    from image_envelope import MAGIC, decode_image

    baseline_path = ROOT / "docs/shared-firmware/baseline.json"
    source_paths = [Path(__file__).resolve(), Path(assembler.__file__).resolve(),
                    baseline_path, Path(read_only_spiffs.__file__).resolve()]
    source_hashes = {str(p): sha(p.read_bytes()) for p in source_paths}
    baseline = json.loads(baseline_path.read_text())
    assembly = json.loads((args.assembly / "assembly.json").read_text())
    app = (args.application / "canonical/ota_update.elf").read_bytes()
    build = json.loads((args.application / "build.json").read_text())
    require(app.startswith(b"\x7fELF"), "Canonical application is not ELF")
    require(len(build["builds"]) == 2 and
            all(r["sha256"] == sha(app) for r in build["builds"]),
            "Canonical repeated build identity differs")
    require(assembly["shared_firmware_update"] == sha(app) and
            set(assembly["products"]) == set(LANES), "Assembly identity differs")
    for asset in baseline["assets"]:
        path = args.downloads / asset["name"]
        require(path.is_file(), "Pinned local download missing: " + asset["name"])
        data = path.read_bytes()
        require(len(data) == asset["size"] and sha(data) == asset["sha256"],
                "Pinned baseline download differs: " + asset["name"])

    # These inputs are actual pinned product bytes, not the assembly receipt's
    # claimed before/after masks. Decode each only once for fault injection.
    stored_inputs, sd_inputs, raw_lane = {}, {}, {}
    for lane in LANES:
        data = (args.downloads / (lane + "-store.bin")).read_bytes()
        stored_inputs[lane] = read_only_spiffs.read_image(data, len(data))
        raw_lane[sha(data)] = lane
        sd_inputs[lane] = zip_tree(args.downloads / (lane + "-sd-apps.zip"))

    expected_after, expected_sd, observed_after, observed_sd = {}, {}, {}, {}
    provider_files = {}
    results = []

    def exact_output(lane, after, sd):
        require(after == expected_after[lane],
                "Unexpected internal bytes or authority: " + lane)
        require(sd == expected_sd[lane], "Unexpected SD bytes or routes: " + lane)

    def test(name, action, rejection=None):
        try:
            action()
        except ValueError as error:
            if rejection is None or rejection not in str(error):
                raise
            results.append({"name": name, "passed": True, "rejected": str(error)})
        else:
            require(rejection is None, "Fault was accepted: " + name)
            results.append({"name": name, "passed": True})
        print("PASS", name, flush=True)

    for lane in LANES:
        before = {n: decode_image(b) if b.startswith(MAGIC) else b
                  for n, b in stored_inputs[lane].items()}
        authority = baseline["authority"][lane]
        old_app = authority["applications"][0]
        old_row = old_app["row"]
        selected = args.application / "selections" / SELECTION[lane]
        selected_manifest_bytes = (selected / "ota_update.json").read_bytes()
        manifest = json.loads(selected_manifest_bytes)
        require((selected / "ota_update.elf").read_bytes() == app and
                sha(selected_manifest_bytes) == build["selections"][SELECTION[lane]]["manifest_sha256"],
                "Selected application identity differs: " + lane)
        require(manifest["id"] == "ota_update" and not manifest["requires"],
                "Optional application contract differs: " + lane)
        old_manifest = json.loads(before[old_row["manifest"]])
        require(old_manifest == old_app["manifest"], "Original app manifest differs")
        after, sd = dict(before), dict(sd_inputs[lane])
        boot = json.loads(before["boot.json"])
        require(old_row in boot["app_capabilities"], "Original app row absent")
        new_row = copy.deepcopy(old_row)
        new_row["grants"] = [g for g in new_row["grants"]
                             if g["capability"] not in ("input.touch.raw", "input.navigation")]
        for capability in ("ui.scene", "time.civil", "app.catalog"):
            grant = {"capability": capability, "api": 1, "instance_id": 0}
            if grant not in new_row["grants"]:
                new_row["grants"].append(grant)
        boot["app_capabilities"][boot["app_capabilities"].index(old_row)] = new_row
        new_manifest = copy.deepcopy(old_manifest)
        for key in ("requires", "optional", "version", "entry", "architecture"):
            new_manifest[key] = copy.deepcopy(manifest[key])
        after[old_row["manifest"]] = encode(new_manifest)
        source = old_row.get("image_source")
        app_path = source["path"] if source else str(PurePosixPath(old_row["manifest"]).parent / old_manifest["file_name"])
        (sd if source else after)[app_path] = app
        if source:
            sd[str(PurePosixPath(app_path).parent / "manifest.json")] = encode(new_manifest)

        original = next(p for p in authority["providers"]
                        if p["manifest"]["id"] == "software-update-firmware")
        provider = args.providers / lane / "software-update-firmware"
        provider_bytes = (provider / "driver.elf").read_bytes()
        provider_manifest = json.loads((provider / "manifest.json").read_text())
        provider_record = json.loads((provider / "build-record.json").read_text())
        policy = {k: v for k, v in provider_record["update_policy"].items() if k != "header"}
        require(policy == authority["provider_policy"] and
                provider_record["sha256"] == sha(provider_bytes), "Provider policy/ELF differs")
        require({k: v for k, v in provider_manifest.items() if k != "version"} ==
                {k: v for k, v in original["manifest"].items() if k != "version"},
                "Provider immutable contract changed")
        p_row = original["row"]
        p_path = str(PurePosixPath(p_row["manifest"]).parent / provider_manifest["file_name"])
        provider_files[lane] = p_path
        after[p_path] = provider_bytes
        after[p_row["manifest"]] = encode(provider_manifest)
        new_provider_row = copy.deepcopy(p_row)
        if p_row.get("image_source"):
            old_path = p_row["image_source"]["path"]
            require(PurePosixPath(old_path).name == "image-" + sha(before[p_path]) + ".elf",
                    "Original provider content-address differs")
            new_path = str(PurePosixPath(old_path).parent / ("image-" + sha(provider_bytes) + ".elf"))
            del sd[old_path]
            sd[new_path] = provider_bytes
            new_provider_row["image_source"]["path"] = new_path
            boot["drivers"][boot["drivers"].index(p_row)] = new_provider_row
        after["boot.json"] = encode(boot)
        expected_after[lane], expected_sd[lane] = after, sd
        directory = args.assembly / lane
        observed_after[lane], observed_sd[lane] = file_tree(directory / "after"), file_tree(directory / "after-sd")

        def qualify(lane=lane, directory=directory, before=before):
            exact_output(lane, observed_after[lane], observed_sd[lane])
            require(file_tree(directory / "before") == before and
                    file_tree(directory / "before-stored") == stored_inputs[lane] and
                    file_tree(directory / "before-sd") == sd_inputs[lane],
                    "Assembly baseline trees differ")
            stored = file_tree(directory / "stored")
            require({n: decode_image(b) if b.startswith(MAGIC) else b for n, b in stored.items()} == expected_after[lane],
                    "Stored canonical decoding differs")
            for name, data in stored_inputs[lane].items():
                if expected_after[lane][name] == before[name]:
                    require(stored[name] == data, "Unrelated stored bytes changed")
            packed = (directory / "store.bin").read_bytes()
            require(read_only_spiffs.read_image(packed, len(packed)) == stored,
                    "Actual packed store differs from validated tree")
            require(zip_tree(directory / "sd-apps.zip") == expected_sd[lane],
                    "Actual SD archive differs from validated tree")
            row = assembly["products"][lane]
            require(sha(packed) == row["store_sha256"] and
                    sha((directory / "sd-apps.zip").read_bytes()) == row["sd_archive_sha256"],
                    "Pack receipt differs from actual bytes")
        test("real-assembly-exact-delta-and-roundtrip-" + lane, qualify)

    # Invoke the actual assembler. Only packing/output-tree emission is replaced
    # in these negative control-flow tests; all authority/policy checks execute.
    def invoke(name, providers=None, fault=None):
        case = args.output / "cases" / name
        case.parent.mkdir(parents=True, exist_ok=True)
        capture = io.StringIO()

        def read_input(data, size):
            lane = raw_lane[sha(data)]
            value = dict(stored_inputs[lane])
            if fault:
                fault(lane, value)
            return value

        argv = [str(Path(assembler.__file__)), "--application", str(args.application),
                "--providers", str(providers or args.providers), "--bootfs-tools", str(args.bootfs_tools),
                "--downloads", str(args.downloads), "--output", str(case)]
        try:
            with patch.object(sys, "argv", argv), patch.object(read_only_spiffs, "read_image", read_input), \
                    patch.object(assembler, "pack_candidate", return_value={"packed": True, "test_only_no_pack": True}), \
                    patch.object(assembler, "write_tree", return_value=None), contextlib.redirect_stdout(capture):
                assembler.main()
        finally:
            (args.output / (name + ".log")).write_text(capture.getvalue())

    test("actual-assembler-control", lambda: invoke("control"))
    for target in LANES:
        for donor in LANES:
            if donor == target:
                continue
            name = "reject-provider-swap-" + target + "-from-" + donor
            fixture = args.output / "providers" / name
            fixture.mkdir(parents=True)
            for lane in LANES:
                (fixture / lane).symlink_to((args.providers / (donor if lane == target else lane)).resolve(), target_is_directory=True)
            test(name, lambda name=name, fixture=fixture: invoke(name, fixture),
                 "Provider policy belongs to another product " + target)

        def missing_catalog(lane, stored, target=target):
            if lane != target:
                return
            boot = json.loads(stored["boot.json"])
            row = next(r for r in boot["app_capabilities"] if r["manifest"] == "contexts.json")
            require(CATALOG in row["grants"], "Injection requires original native grant")
            row["grants"].remove(CATALOG)
            stored["boot.json"] = encode(boot)
        name = "reject-missing-native-catalog-" + target
        test(name, lambda name=name: invoke(name, fault=missing_catalog),
             "Existing native catalog authority not established")

        # Mutate unrelated output bytes; the independent oracle does not trust
        # changed_internal/changed_sd receipt allowlists.
        before = {n: decode_image(b) if b.startswith(MAGIC) else b for n, b in stored_inputs[target].items()}
        untouched = next(n for n in before if observed_after[target][n] == before[n] and n != "boot.json")
        tampered = dict(observed_after[target]); tampered[untouched] += b"unrelated-corruption"
        test("reject-unrelated-internal-mutation-" + target,
             lambda target=target, tampered=tampered: exact_output(target, tampered, observed_sd[target]),
             "Unexpected internal bytes")

    def wrong_original_digest(lane, stored):
        if lane == "x4":
            stored[provider_files[lane]] += b"digest-mismatch"
    test("reject-original-provider-content-digest",
         lambda: invoke("reject-original-provider-content-digest", fault=wrong_original_digest),
         "Original content-addressed fallback differs")

    tampered = dict(observed_sd["x4"])
    unchanged = next(n for n, b in sd_inputs["x4"].items() if tampered.get(n) == b)
    tampered[unchanged] += b"unrelated-corruption"
    test("reject-unrelated-sd-mutation", lambda: exact_output("x4", observed_after["x4"], tampered),
         "Unexpected SD bytes")
    tampered = dict(observed_after["x4"])
    boot = json.loads(tampered["boot.json"])
    # Identify the selected provider by its manifest content, never row order.
    provider_row = next(r for r in boot["drivers"] if json.loads(tampered[r["manifest"]])["id"] == "software-update-firmware")
    provider_row["image_source"]["path"] = "Drivers/software-update-firmware/image-" + "0" * 64 + ".elf"
    tampered["boot.json"] = encode(boot)
    test("reject-candidate-provider-route-digest", lambda: exact_output("x4", tampered, observed_sd["x4"]),
         "Unexpected internal bytes")

    for path, digest in source_hashes.items():
        require(sha(Path(path).read_bytes()) == digest, "Qualification source changed during run: " + path)
    receipt = {
        "schema": 1, "passed": True, "tests": results,
        "source_sha256": source_hashes,
        "assembly_sha256": sha((args.assembly / "assembly.json").read_bytes()),
        "application_sha256": sha(app),
        "products": {lane: {"store_sha256": sha((args.assembly / lane / "store.bin").read_bytes()),
                            "sd_archive_sha256": sha((args.assembly / lane / "sd-apps.zip").read_bytes())} for lane in LANES},
        "scope": "Actual pinned baseline and actual packed outputs independently decoded; exact allowed app/provider/authority delta; real assembler control flow with decoded-store fault injection and no repeated packing. No hardware, network, publication, or target execution.",
    }
    (args.output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(str(len(results)) + " assembly qualification cases passed; receipt: " + str(args.output / "receipt.json"))


if __name__ == "__main__":
    main()

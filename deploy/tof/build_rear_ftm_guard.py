#!/usr/bin/env python3
"""Build an isolated, one-file rear FTM guard on the exact audited ToF image."""
import argparse
import datetime
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parents[2]
BASE = "sha256:b4046480b31b9648ade0b852d42f7d0fd6de9d228297d1dd205763ff62ca108c"
BASE_SOURCE = "d2ea6b81a2e60259e7fd6c95364b658a96594bee4ece009d1f00ea9476fc4ad8"
SOURCE = "firmware/esp32c3-tof-anchor/tools/tof_dual_radio.py"
TEST = "firmware/esp32c3-tof-anchor/tools/test_tof_dual_radio.py"


def run(argv, **kwargs):
    return subprocess.check_output(argv, text=True, **kwargs)


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--docker", default="docker")
    args = parser.parse_args()
    revision = run(["git", "-C", str(ROOT), "rev-parse", "HEAD"]).strip()
    tree = run(["git", "-C", str(ROOT), "rev-parse", "HEAD^{tree}"]).strip()
    lock = json.loads(run(["git", "-C", str(ROOT), "show", revision + ":deploy/tof/source-lock.json"]))
    files = {}
    for path in (SOURCE, TEST):
        committed = subprocess.check_output(["git", "-C", str(ROOT), "show", revision + ":" + path])
        if committed != (ROOT / path).read_bytes() or sha(committed) != lock["files"][path]:
            raise ValueError("Committed source/test differs from the reviewed lock: " + path)
        files[path] = committed
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    docker = args.docker
    base = json.loads(run([docker, "image", "inspect", BASE]))[0]
    if base["Id"] != BASE or base["Os"] + "/" + base["Architecture"] != "linux/arm64":
        raise ValueError("The exact audited local arm64 image is required; this script never pulls")
    limits = ["--rm", "--network", "none", "--read-only", "--tmpfs",
              "/tmp:rw,nosuid,nodev,size=64m", "--cap-drop", "ALL",
              "--security-opt", "no-new-privileges", "--memory", "512m", "--cpus", "1",
              "--pids-limit", "128"]
    observed = run([docker, "run", *limits, "--entrypoint", "python", BASE, "-c",
                    "import hashlib,pathlib;print(hashlib.sha256(pathlib.Path('/app/tof_dual_radio.py').read_bytes()).hexdigest())"]).strip()
    if observed != BASE_SOURCE:
        raise ValueError("Audited base source hash mismatch")
    base_tag = "biomerte-tof:guard-base-" + BASE.split(":")[1]
    tagged = subprocess.run([docker, "image", "inspect", base_tag], capture_output=True, text=True)
    if tagged.returncode == 0 and json.loads(tagged.stdout)[0]["Id"] != BASE:
        raise ValueError("Reserved local base tag points at another image")
    subprocess.run([docker, "image", "tag", BASE, base_tag], check=True)
    context = out / "context"
    context.mkdir()
    (context / "tof_dual_radio.py").write_bytes(files[SOURCE])
    (context / "tof_dual_radio.py").chmod(0o644)
    epoch = int(run(["git", "-C", str(ROOT), "show", "-s", "--format=%ct", revision]).strip())
    os.utime(context / "tof_dual_radio.py", (epoch, epoch))
    labels = {
        "org.biomerte.patch.revision": revision,
        "org.biomerte.patch.source": "https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps",
        "org.biomerte.patch.base-image": BASE,
        "org.biomerte.patch.path": "/app/tof_dual_radio.py",
        "org.biomerte.patch.sha256": sha(files[SOURCE]),
    }
    recipe = "FROM " + base_tag + "\nCOPY tof_dual_radio.py /app/tof_dual_radio.py\n"
    recipe += "".join("LABEL " + key + "=" + json.dumps(value) + "\n" for key, value in labels.items())
    (context / "Dockerfile").write_text(recipe)
    tag = "biomerte-tof:rear-disabled-" + revision[:12]
    with (out / "build.log").open("w") as log:
        subprocess.run([docker, "build", "--pull=false", "--network", "none",
                        "--iidfile", str(out / "image-id"), "-t", tag, str(context)],
                       env=dict(os.environ, DOCKER_BUILDKIT="0"), stdout=log,
                       stderr=subprocess.STDOUT, check=True)
    image = (out / "image-id").read_text().strip()
    candidate = json.loads(run([docker, "image", "inspect", image]))[0]
    if candidate["RootFS"]["Layers"][:-1] != base["RootFS"]["Layers"]:
        raise ValueError("Candidate must add exactly one filesystem layer to the audited base")
    for key in set(base["Config"]) | set(candidate["Config"]):
        if key not in ("Labels", "Image") and base["Config"].get(key) != candidate["Config"].get(key):
            raise ValueError("Unexpected runtime image configuration change: " + key)
    if candidate["Config"].get("Labels", {}) != dict(base["Config"].get("Labels") or {}, **labels):
        raise ValueError("Original image provenance must be preserved")
    test_log = out / "tests.log"
    with test_log.open("w") as log:
        subprocess.run([docker, "run", *limits,
                        "--mount", "type=bind,src=" + str(ROOT / TEST) + ",dst=/app/test_tof_dual_radio.py,readonly",
                        "--entrypoint", "python", image, "-m", "unittest",
                        "discover", "-s", "/app", "-p", "test*.py", "-v"],
                       stdout=log, stderr=subprocess.STDOUT, check=True)
    archive = out / "image.tar"
    subprocess.run([docker, "image", "save", "--output", str(archive), image], check=True)
    with tarfile.open(archive) as saved:
        manifest = json.load(saved.extractfile("manifest.json"))
        if len(manifest) != 1:
            raise ValueError("Expected one exported candidate image")
        layer_bytes = saved.extractfile(manifest[0]["Layers"][-1]).read()
    with tarfile.open(fileobj=io.BytesIO(layer_bytes)) as layer:
        changed = [entry for entry in layer if not entry.isdir()]
        if len(changed) != 1 or changed[0].name.lstrip("./") != "app/tof_dual_radio.py" or not changed[0].isfile():
            raise ValueError("Overlay changes more than the one reviewed production file")
        if sha(layer.extractfile(changed[0]).read()) != sha(files[SOURCE]):
            raise ValueError("Exported source differs from reviewed source")
    digest = hashlib.sha256()
    with archive.open("rb") as stream:
        for block in iter(lambda: stream.read(4 * 1024 * 1024), b""):
            digest.update(block)
    receipt = {
        "schema": "biomerte.tof-one-file-overlay/1",
        "created_at": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "source_commit": revision, "source_tree": tree, "base_image_id": BASE,
        "base_source_sha256": BASE_SOURCE, "image_id": image, "local_tag": tag,
        "platform": "linux/arm64", "changed_production_files": ["/app/tof_dual_radio.py"],
        "source_sha256": sha(files[SOURCE]), "test_sha256": sha(files[TEST]),
        "dockerfile_sha256": sha(recipe.encode()), "patch_labels": labels,
        "base_layers_preserved": True, "runtime_image_config_preserved": True,
        "tests_exit_code": 0, "tests_log_sha256": sha(test_log.read_bytes()),
        "tests_tail": "\n".join(test_log.read_text().splitlines()[-5:]),
        "archive": str(archive), "archive_bytes": archive.stat().st_size,
        "archive_sha256": digest.hexdigest(),
        "production_activated": False, "registry_pushed": False,
    }
    (out / "candidate-receipt.json").write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n")
    print(json.dumps(receipt, sort_keys=True), flush=True)


if __name__ == "__main__":
    main()

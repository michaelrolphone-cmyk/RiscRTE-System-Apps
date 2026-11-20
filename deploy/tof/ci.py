#!/usr/bin/env python3
"""Qualify this ToF image from exact source archives; never publish or activate."""
import argparse
import datetime
import hashlib
import io
import json
from pathlib import Path
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parents[2]

def command(argv, *, cwd=None, timeout=300):
    result = subprocess.run(argv, cwd=cwd, capture_output=True, text=True, timeout=timeout)
    if result.returncode:
        raise RuntimeError("Command failed (exit %s): %s\n%s" % (
            result.returncode, argv[0], result.stderr[-4000:]))
    return result.stdout

def blob(repo, revision, path):
    return subprocess.check_output(["git", "-C", str(repo), "show", revision + ":" + path],
                                   timeout=30)

def archive(repo, revision, paths, destination):
    raw = subprocess.check_output(["git", "-C", str(repo), "archive", "--format=tar",
                                   revision, *paths], timeout=30)
    destination.mkdir(parents=True, exist_ok=False)
    with tarfile.open(fileobj=io.BytesIO(raw)) as source:
        for entry in source.getmembers():
            path = Path(entry.name)
            if path.is_absolute() or ".." in path.parts or entry.issym() or entry.islnk():
                raise ValueError("Unsafe Git archive entry")
        source.extractall(destination)
    return dict(revision=revision, sha256=hashlib.sha256(raw).hexdigest(), bytes=len(raw))

def sha_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(4 * 1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--core-repo", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--docker", default="docker")
    parser.add_argument("--platform", choices=("linux/amd64", "linux/arm64"),
                        help="Explicit build/test platform; default is the Docker server platform")
    parser.add_argument("--verify-only", action="store_true")
    args = parser.parse_args()
    revision = command(["git", "rev-parse", "HEAD"], cwd=ROOT).strip()
    lock = json.loads(blob(ROOT, revision, "deploy/tof/source-lock.json"))
    if lock.get("schema") != "biomerte.tof-source-lock/1":
        raise ValueError("Unsupported source lock")
    for name, expected in lock["files"].items():
        if hashlib.sha256(blob(ROOT, revision, name)).hexdigest() != expected:
            raise ValueError("Captured source changed without a new qualification: " + name)
    core = lock["core"]
    shared = blob(args.core_repo, core["ci_revision"], core["client_path"])
    if hashlib.sha256(shared).hexdigest() != core["client_sha256"]:
        raise ValueError("Shared host I/O client differs from reviewed source")
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    proof = dict(schema="biomerte.tof-ci-source/1", source_repository=lock["source_repository"],
                 source_revision=revision, captured_image_source_revision=lock["captured_image_source_revision"],
                 core=core, captured_file_count=len(lock["files"]))
    (out / "source-proof.json").write_text(json.dumps(proof, sort_keys=True, indent=2) + "\n")
    if args.verify_only:
        print(json.dumps(proof, sort_keys=True))
        return
    docker = args.docker
    platform = args.platform or command([docker, "version", "--format",
                        "{{.Server.Os}}/{{.Server.Arch}}"]).strip()
    if platform not in lock["bases"]:
        raise ValueError("No qualified binary-wheel/base platform: " + platform)
    base = lock["bases"][platform]
    contexts = out / "contexts"
    contexts.mkdir()
    source_context, core_context = contexts / "source", contexts / "core"
    archives = dict(
        source=archive(ROOT, revision, ["firmware/esp32c3-tof-anchor/tools", "deploy/tof"], source_context),
        core=archive(args.core_repo, core["ci_revision"], [core["client_path"]], core_context))
    tag = "biomerte-tof:ci-" + revision[:12] + "-" + platform.rsplit("/", 1)[-1]
    argv = [docker, "build", "--platform", platform,
            "--build-context", "host_io=" + str(core_context),
            "--build-arg", "PYTHON_BASE=" + base,
            "--build-arg", "SOURCE_REVISION=" + revision,
            "--build-arg", "HOST_IO_REVISION=" + core["ci_revision"],
            "--label", "org.biomerte.host-io.source=https://github.com/" + core["repository"],
            "--iidfile", str(out / "image-id"), "-f", str(source_context / "deploy/tof/Dockerfile"),
            "-t", tag, str(source_context)]
    with (out / "build.log").open("w") as log:
        result = subprocess.run(argv, stdout=log, stderr=subprocess.STDOUT, timeout=300)
    if result.returncode:
        raise RuntimeError("ToF build failed; see build.log")
    image = (out / "image-id").read_text().strip()
    limits = ["--platform", platform, "--rm", "--network", "none", "--read-only", "--tmpfs",
              "/tmp:rw,nosuid,nodev,size=64m", "--cap-drop", "ALL",
              "--security-opt", "no-new-privileges", "--memory", "512m",
              "--cpus", "1", "--pids-limit", "128"]
    command([docker, "run", *limits, "--entrypoint", "python", image,
             "/app/tof_coordinator.py", "--help"], timeout=30)
    result = subprocess.run([docker, "run", *limits, "--entrypoint", "python", image,
                             "-m", "unittest", "discover", "-s", "/app", "-p", "test*.py"],
                            capture_output=True, text=True, timeout=60)
    (out / "tests.log").write_text(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError("ToF isolated image tests failed; see tests.log")
    inspected = json.loads(command([docker, "image", "inspect", image]))[0]
    labels = inspected["Config"]["Labels"]
    expected_labels = {
        "org.opencontainers.image.source": lock["source_repository"],
        "org.opencontainers.image.revision": revision,
        "io.biomerte.image": "tof-coordinator",
        "io.biomerte.platform": platform,
    }
    if any(labels.get(key) != value for key, value in expected_labels.items()):
        raise ValueError("Image source or common release label mismatch")
    if labels.get("org.biomerte.host-io.revision") != core["ci_revision"]:
        raise ValueError("Image secondary source label mismatch")
    if inspected["Os"] + "/" + inspected["Architecture"] != platform:
        raise ValueError("Built image platform mismatch")
    export = out / "image.tar"
    command([docker, "image", "save", "--output", str(export), image], timeout=120)
    receipt = dict(schema="biomerte.tof-ci-image/1", platform=platform, image_id=image,
                   local_tag=tag, base_platform_manifest=base, source_archives=archives,
                   source=proof, oci_labels=labels, image_size_bytes=inspected["Size"],
                   tests_exit_code=result.returncode, tests_summary=result.stderr.strip(),
                   export_sha256=sha_file(export), export_bytes=export.stat().st_size,
                   production_activated=False, registry_pushed=False)
    (out / "image-receipt.json").write_text(json.dumps(receipt, sort_keys=True, indent=2) + "\n")
    # Use the shared BiomeRTE verify/publish/render commands for registry handoff.
    release = dict(schema="biomerte-container-release/1", source_commit=revision,
                   source_url=lock["source_repository"],
                   created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                   complete=True, images=[dict(
                       id="tof-coordinator", platform=platform, image_id=image, tag=tag,
                       dockerfile="deploy/tof/Dockerfile", base_image=base,
                       archive=dict(file=export.name, sha256=receipt["export_sha256"],
                                    bytes=receipt["export_bytes"]), registry_ref=None)])
    (out / "images.lock.json").write_text(json.dumps(release, sort_keys=True, indent=2) + "\n")
    print(json.dumps(receipt, sort_keys=True))

if __name__ == "__main__":
    main()

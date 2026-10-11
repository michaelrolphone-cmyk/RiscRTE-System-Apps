#!/usr/bin/env python3
"""Independent hashlib/OpenSSL oracle and host/optional Xtensa Digest test runner."""
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def quote(text):
    return '"' + text.replace('\\', '\\\\').replace('"', '\\"') + '"'


def vector(*, username="Mufasa", password="Circle of Life", realm="http-auth@example.org",
           nonce=None, method="GET", uri="/dir/index.html", qop="auth", count=1,
           cnonce="f2/wE4q74E6zIJEtWaHKaf5wv/H5QzzpXusqGemxURZJ", body=b""):
    if nonce is None:
        nonce = base64.b64decode("7ypf/xlj9XXwfDPEoM4URrv/xwf94BcCAzFZH4GiTo0v")
    nonce_text = base64.b64encode(nonce).decode()
    nc = f"{count:08x}"
    digest = lambda data: hashlib.sha256(data).hexdigest()
    ha1 = digest(f"{username}:{realm}:{password}".encode())
    a2 = f"{method}:{uri}"
    if qop == "auth-int":
        a2 += ":" + digest(body)
    ha2 = digest(a2.encode())
    response = digest(f"{ha1}:{nonce_text}:{nc}:{cnonce}:{qop}:{ha2}".encode())
    fields = [("username", quote(username)), ("realm", quote(realm)), ("uri", quote(uri)),
              ("algorithm", "SHA-256"), ("nonce", quote(nonce_text)), ("nc", nc),
              ("cnonce", quote(cnonce)), ("qop", qop), ("response", quote(response))]
    authorization = "Digest " + ", ".join(f"{key}={value}" for key, value in fields)
    values = [username, password, realm, nonce.hex(), method, uri, authorization, body.hex()]
    return "{" + ",".join(json.dumps(v) for v in values) + f",{str(qop == 'auth-int').lower()}" + "}", response


def fixtures(path):
    # RFC 7616 section 3.9.1 SHA-256 vector. Opaque is not hashed and this server
    # does not advertise it, so the oracle omits that optional directive.
    rfc, response = vector()
    assert response == "753927fa0e85d155564e2e272a28d1802ca10daf4496794697cf8db5856cb6c1"
    all_vectors = [rfc]
    for qop in ("auth", "auth-int"):
        for size in (0, 1, 55, 56, 63, 64, 65, 127, 128, 4096, 65536):
            data = bytes((i * 37 + 3) & 255 for i in range(size))
            all_vectors.append(vector(qop=qop, method="PUT", uri="/file%20name.txt", body=data)[0])
    for nonce_size in (16, 17, 18, 32, 47, 48):
        all_vectors.append(vector(nonce=bytes(range(nonce_size)))[0])
    all_vectors.append(vector(username='a"b\\c', password="p:a,ss\\word", realm="local files",
                              uri="/a%2Fb,a.txt", cnonce='x"y\\z', qop="auth-int", body=b"a\x00b")[0])
    all_vectors.append(vector(username="u" * 64, password="p" * 256, realm="r" * 64,
                              uri="/" + "a" * 767, cnonce="c" * 96, qop="auth-int")[0])
    text = "static const Vector vectors[] = {\n" + ",\n".join(all_vectors) + "\n};\n"
    text += "static const Vector integrityVector = " + vector(qop="auth-int", method="PUT", body=b"test\x00body")[0] + ";\n"
    counts = [vector(count=n)[0] for n in (1, 2, 3, 0xffffffff)]
    counts.append(vector(count=1, cnonce="second-client")[0])
    text += "static const Vector countVectors[] = {" + ",\n".join(counts) + "};\n"
    capacity = [vector(cnonce=f"client-{i}")[0] for i in range(9)]
    text += "static const Vector capacityVectors[] = {" + ",\n".join(capacity) + "};\n"
    text += "static const Vector capacityNext = " + vector(cnonce="client-0", count=2)[0] + ";\n"
    text += "static const struct {const char* inputHex; const char* expectedHex;} hashVectors[] = {\n"
    inputs = [b"", b"abc", b"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"]
    inputs += [bytes((i * 19) & 255 for i in range(size)) for size in (1, 55, 56, 63, 64, 65, 127, 128, 129, 4096)]
    for data in inputs:
        expected = hashlib.sha256(data).hexdigest()
        openssl = subprocess.check_output(["openssl", "dgst", "-sha256", "-binary"], input=data).hex()
        assert openssl == expected
        text += "{" + json.dumps(data.hex()) + "," + json.dumps(expected) + "},\n"
    text += "};\n"
    path.write_text(text)


def run(command):
    subprocess.run([str(v) for v in command], cwd=ROOT, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--toolchain", type=Path, help="Optional Xtensa toolchain bin directory")
    args = parser.parse_args()
    out = (args.output or Path(tempfile.mkdtemp(prefix="webdav-digest-"))).resolve()
    out.mkdir(parents=True, exist_ok=True)
    fixtures(out / "webdav_digest_vectors.inc")
    sha_source = "lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c"
    common = ["-g", "-Wall", "-Wextra", "-Werror"]
    if os.environ.get("SANITIZE", "0") == "1":
        common += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-fno-pie", "-no-pie"]
    run([os.environ.get("CC", "cc"), "-std=c99", *common, "-c", sha_source, "-o", out / "sha2.o"])
    run([os.environ.get("CXX", "c++"), "-std=c++17", *common, "-Ilib/RemoteFiles", "-I" + str(out),
         "lib/RemoteFiles/WebDavDigest.cpp", "test/remote_files/webdav_digest_test.cpp", out / "sha2.o",
         "-o", out / "webdav-digest-test"])
    run([out / "webdav-digest-test"])
    if args.toolchain:
        flags = ["-Os", "-mlongcalls", "-fPIC", "-fno-common", "-ffunction-sections", "-fdata-sections", "-Wall", "-Wextra", "-Werror"]
        run([args.toolchain / "xtensa-esp32s3-elf-gcc", "-std=c99", *flags, "-c", sha_source, "-o", out / "sha2-xtensa.o"])
        run([args.toolchain / "xtensa-esp32s3-elf-g++", "-std=c++17", *flags, "-fno-exceptions", "-fno-rtti", "-c",
             "lib/RemoteFiles/WebDavDigest.cpp", "-o", out / "digest-xtensa.o"])
        run([args.toolchain / "xtensa-esp32s3-elf-ld", "-r", out / "sha2-xtensa.o", out / "digest-xtensa.o", "-o", out / "digest-combined-xtensa.o"])
        imports = subprocess.check_output([str(args.toolchain / "xtensa-esp32s3-elf-nm"), "-u", str(out / "digest-combined-xtensa.o")], text=True)
        names = {line.split()[-1] for line in imports.splitlines()}
        assert names <= {"memcpy", "memset", "strlen", "strchr", "strcmp", "snprintf"}, names
        receipt = {"kind": "Xtensa Digest compile and bounded libc imports; no device execution", "imports": sorted(names),
                   "objects": {p.name: {"bytes": p.stat().st_size, "sha256": hashlib.sha256(p.read_bytes()).hexdigest()}
                               for p in out.glob("*-xtensa.o")}}
        (out / "xtensa-receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
        print("Xtensa SHA-256 Digest compile/import check PASS: " + ", ".join(sorted(names)))


if __name__ == "__main__":
    main()

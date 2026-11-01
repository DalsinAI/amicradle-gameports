#!/usr/bin/env python3
"""Neverball release package verifier.

This is deliberately strict: it verifies package completeness and provenance.
It does not turn a package into RELEASE; runtime qualification remains mandatory.
"""
from __future__ import annotations
import argparse, hashlib, json, zipfile
from pathlib import Path, PurePosixPath

PIN = "a1ed09911dca262d80049c12a2824d683af494d6"
REQUIRED = (
    "Neverball",
    "Neverputt",
    "LICENSE.md",
    "OPENUP-RELEASE.txt",
    "data",
    "doc",
)

def sha256(path: Path) -> str:
    h=hashlib.sha256()
    with path.open("rb") as f:
        for b in iter(lambda:f.read(1024*1024), b""):
            h.update(b)
    return h.hexdigest()

def verify_inventory(root: Path) -> list[str]:
    """Return failures for the package's complete, self-excluding hash inventory."""
    inventory = root / "SHA256SUMS"
    if not inventory.is_file():
        return ["missing SHA256SUMS"]

    failures: list[str] = []
    entries: dict[str, str] = {}
    for number, raw in enumerate(inventory.read_text(errors="replace").splitlines(), 1):
        fields = raw.split(maxsplit=1)
        if len(fields) != 2 or len(fields[0]) != 64:
            failures.append(f"malformed SHA256SUMS line {number}")
            continue
        digest, name = fields
        name = name.lstrip("*")
        while name.startswith("./"):
            name = name[2:]
        path = PurePosixPath(name)
        if not name or path.is_absolute() or ".." in path.parts:
            failures.append(f"unsafe SHA256SUMS path on line {number}")
            continue
        normalized = path.as_posix()
        if normalized == "SHA256SUMS":
            failures.append("SHA256SUMS must not hash itself")
            continue
        if normalized in entries:
            failures.append(f"duplicate SHA256SUMS entry: {normalized}")
            continue
        if any(c not in "0123456789abcdef" for c in digest):
            failures.append(f"invalid SHA-256 digest for {normalized}")
            continue
        entries[normalized] = digest

    files = {
        path.relative_to(root).as_posix(): path
        for path in root.rglob("*")
        if path.is_file() and path != inventory
    }
    missing = sorted(set(files) - set(entries))
    extra = sorted(set(entries) - set(files))
    if missing:
        failures.append("SHA256SUMS missing files: " + ", ".join(missing))
    if extra:
        failures.append("SHA256SUMS lists absent files: " + ", ".join(extra))
    for name in sorted(set(files) & set(entries)):
        if sha256(files[name]) != entries[name]:
            failures.append(f"SHA256 mismatch: {name}")
    return failures

def main() -> int:
    ap=argparse.ArgumentParser()
    ap.add_argument("package", type=Path)
    ap.add_argument("--report", type=Path)
    a=ap.parse_args()
    root=a.package.resolve()
    failures=[]
    for name in REQUIRED:
        if not (root/name).exists():
            failures.append(f"missing {name}")
    failures.extend(verify_inventory(root))
    note=root/"OPENUP-RELEASE.txt"
    if note.is_file() and PIN not in note.read_text(errors="replace"):
        failures.append("release note does not carry the pinned upstream commit")
    data=root/"data"
    if data.is_dir():
        sols=list(data.rglob("*.sol"))
        pk3=data/"data-1.6.0.pk3"
        pk3_sols=0
        if pk3.is_file():
            if sha256(pk3) != "b2eddfbe05443e36719541639cb6246c5dd81260bce67a053d0d2c0ad34bc58c":
                failures.append("official data-1.6.0.pk3 hash mismatch")
            else:
                with zipfile.ZipFile(pk3) as zf:
                    pk3_sols=sum(1 for n in zf.namelist() if n.lower().endswith(".sol"))
        if not sols and pk3_sols == 0:
            failures.append("no compiled .sol level assets found")
    legal=root/"doc"/"legal"
    if not legal.is_dir():
        failures.append("upstream third-party legal notices missing")
    files=sorted(p for p in root.rglob("*") if p.is_file())
    report={
        "schema":1,
        "port":"neverball",
        "upstreamCommit":PIN,
        "package":str(root),
        "fileCount":len(files),
        "bytes":sum(p.stat().st_size for p in files),
        "failures":failures,
        "packageGate":"PASS" if not failures else "FAIL",
        "release":False,
        "releaseReason":"Runtime qualification is a separate mandatory gate."
    }
    if a.report:
        a.report.parent.mkdir(parents=True, exist_ok=True)
        a.report.write_text(json.dumps(report,indent=2,sort_keys=True)+"\n")
    print(json.dumps(report,indent=2,sort_keys=True))
    return 0 if not failures else 1

if __name__=="__main__":
    raise SystemExit(main())

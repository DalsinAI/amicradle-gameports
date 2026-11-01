#!/usr/bin/env python3
"""Reject GCC 16's unsafe 68040 FPCR-indexed float stores.

Adapted from OpenRTG's MIT-licensed fpcr_check.py so every Neverball release
build runs the SDK-mandated final-binary gate without relying on an external
source checkout.
"""
from __future__ import annotations

import re
import subprocess
import sys

FUNCTION = re.compile(r"^[0-9a-f]+ <(.*)>:")
SAVE = re.compile(r"fmovel %?fpcr,%?(d[0-7])")
STORE = re.compile(r"fmovel %?fp[0-7],")


def scan(objdump: str, path: str) -> list[tuple[str, str]]:
    result = subprocess.run(
        [objdump, "-m", "m68k:68040", "-d", path],
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode:
        raise RuntimeError(f"{objdump} could not disassemble {path} (exit {result.returncode})")

    found: list[tuple[str, str]] = []
    saved = None
    function = "?"
    for line in result.stdout.splitlines():
        match = FUNCTION.match(line)
        if match:
            function, saved = match.group(1), None
            continue
        match = SAVE.search(line)
        if match:
            saved = match.group(1)
            continue
        if saved and STORE.search(line):
            address = line.split("fmovel", 1)[1].split(",", 1)[1]
            if saved in address:
                found.append((function, line.strip()))
            saved = None
    return found


def main() -> int:
    if len(sys.argv) < 3:
        print("usage: check_m68040_fpcr.py OBJDUMP FILE...", file=sys.stderr)
        return 2
    bad = 0
    for path in sys.argv[2:]:
        for function, instruction in scan(sys.argv[1], path):
            print(f"{path}: {function}: {instruction}")
            bad += 1
    print(f"fpcr_check: {bad} unsafe store(s)" if bad else "fpcr_check: none")
    return 1 if bad else 0


if __name__ == "__main__":
    raise SystemExit(main())

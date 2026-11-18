#!/usr/bin/env python3
"""Build a host-only Deep Drop test using a separate SDK Preview checkout."""

import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
SOURCE = "Текст из файла\r\nВторая строка\n".encode("utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk-dir", type=Path, required=True, help="path to sdk/ in a separate SDK checkout")
    parser.add_argument("--recovery", action="store_true", help="also verify a real worker crash on a POSIX host")
    args = parser.parse_args()
    if args.recovery and os.name != "posix":
        parser.error("--recovery requires a POSIX host")
    sdk = args.sdk_dir.resolve()
    callback = sdk / "examples/presentations/main.c"
    header = sdk / "include/styleos/sdk_v0.h"
    if not callback.is_file() or not header.is_file():
        parser.error("--sdk-dir needs the Presentations example and sdk_v0.h")

    with tempfile.TemporaryDirectory(prefix="styleos-deep-drop-") as tmp:
        scratch = Path(tmp)
        source = scratch / "selected.txt"
        source.write_bytes(SOURCE)
        cases = ["test_deep_drop"]
        if args.recovery:
            cases.append("test_deep_drop_recovery")
        for case in cases:
            binary = scratch / case
            subprocess.run([
                *shlex.split(os.environ.get("CC", "cc")), "-std=c11", "-Wall", "-Wextra", "-Werror",
                "-pedantic", "-I", str(ROOT / "include"), "-I", str(sdk / "include"),
                str(ROOT / "kernel/deep_drop.c"), str(ROOT / "tests" / f"{case}.c"),
                str(callback), "-o", str(binary),
            ], check=True)
            subprocess.run([str(binary), str(source)], check=True, timeout=15)
            if source.read_bytes() != SOURCE:
                raise RuntimeError(f"the selected file changed during {case}")
            print(f"PASS selected file remains byte-identical after {case}", flush=True)


if __name__ == "__main__":
    main()

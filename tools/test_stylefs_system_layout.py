#!/usr/bin/env python3
"""Exercise explicit system-image provisioning with the real v2 decoder.
No live disks; all paths are newly created test artifacts."""
import argparse
import hashlib
from pathlib import Path
import subprocess

ROOT = ("StyleOS", "Program Files", "ProgramData", "Users", "Recovery", "Temp")
PROFILE = ("Desktop", "Documents", "Downloads", "Pictures", "Music", "Videos", "Projects")

def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()

def run(tool, *args, ok=True):
    p = subprocess.run([str(tool), *map(str, args)], capture_output=True, text=True)
    if (p.returncode == 0) != ok:
        raise RuntimeError(f"{args}: {p.returncode} {p.stdout} {p.stderr}")
    return p.stdout

def listed(tool, image, path):
    return [tuple(line.split("\t")) for line in run(tool, "list", image, path).splitlines()]

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--tool", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    a = p.parse_args()
    tool = a.tool.resolve(strict=True)
    out = a.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    image = out / "system.raw"
    legacy = out / "legacy.raw"
    invalid = out / "invalid.raw"
    foreign = out / "foreign.raw"
    run(tool, "new-system", invalid, 8, ok=False)
    if invalid.exists(): raise ValueError("invalid image published")
    foreign.write_bytes(b"foreign existing data")
    before = digest(foreign)
    run(tool, "new-system", foreign, 32, ok=False)
    if digest(foreign) != before: raise ValueError("existing foreign image replaced")
    run(tool, "new-system", image, 32)
    if not run(tool, "info", image).startswith("StyleFS v2\n"): raise ValueError("format")
    if image.stat().st_size != 32*1024*1024: raise ValueError("capacity")
    before = digest(image)
    if listed(tool, image, "/") != [("d", "0", n) for n in ROOT]:
        raise ValueError("actual system root")
    if listed(tool, image, "/Users") != [("d", "0", "Public")]:
        raise ValueError("invented user accounts or missing shared folder")
    for n in ("StyleOS", "Program Files", "ProgramData", "Recovery", "Temp", "Users/Public"):
        if listed(tool, image, "/" + n): raise ValueError("unexpected preinstalled content")
    if digest(image) != before: raise ValueError("read-only listing changed disk")
    run(tool, "new-system", image, 32, ok=False)
    if digest(image) != before: raise ValueError("existing system disk replaced")
    # Explicit test-only profile directory (not account provisioning).
    run(tool, "mkdir", image, "/Users/TestProfile")
    for n in PROFILE: run(tool, "mkdir", image, "/Users/TestProfile/" + n)
    if listed(tool, image, "/Users/TestProfile") != [("d", "0", n) for n in PROFILE]:
        raise ValueError("profile folders")
    payload = out / "note.txt"
    payload.write_bytes("StyleOS — проверка данных\n".encode())
    run(tool, "put", image, "/Users/TestProfile/Documents/note.txt", payload)
    recovered = out / "reopened.txt"
    run(tool, "get", image, "/Users/TestProfile/Documents/note.txt", recovered)
    if recovered.read_bytes() != payload.read_bytes(): raise ValueError("persistent bytes")
    # Existing default command and its established shortcuts are unchanged.
    run(tool, "new", legacy, 32)
    if listed(tool, legacy, "/") != [("d","0",n) for n in ("Документы","Проекты","Загрузки","Музыка","Изображения","Видео")]:
        raise ValueError("legacy root changed")
    if list(out.glob("*.stylefs-*")): raise ValueError("temporary image leak")
    print("SYSTEM LAYOUT PASS: real v2 roots, no fictional accounts, explicit profile directories, persistence, no overwrite, legacy preserved")

if __name__ == "__main__":
    main()

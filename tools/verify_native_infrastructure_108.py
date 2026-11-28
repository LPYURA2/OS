#!/usr/bin/env python3
"""Fail closed unless every required infrastructure job reports success."""
import argparse
import json
import os
from pathlib import Path

EXPECTED = frozenset((
    "deep-drop-host",
    "native-image-75-host",
    "stop-host",
    "hang-regression",
    "source",
    "forced-stop-guest",
    "native-expanded-75-guest",
    "native-expanded-76-guest",
    "native-sdk-77-host",
    "native-sdk-77-guest",
    "native-graphics-78-host",
    "native-surface-79-host",
    "native-patch-80-host",
    "native-patch-81-guest",
    "native-text-84-guest",
    "native-file-85-host",
    "native-file-86-host",
    "native-file-87-host",
    "native-file-89-host",
    "native-file-90-host",
    "native-file-93-host",
    "native-file-95-host",
    "native-file-96-host",
    "native-file-97-host",
    "native-file-98-host",
    "native-file-99-host",
    "native-file-100-host",
    "native-file-101-host",
    "native-file-102-host",
    "ordinary-shell",
    "stylefs-live-regression",
    "waiting-stop-guest",
    "native-file-92-guest",
    "native-file-94-guest",
    "native-tea-107-guest",
    "native-tea-106-host",
    "native-tea-104-host",
    "native-file-103-guest",
    "fault-acceptance",
))


def verify(needs):
    if not isinstance(needs, dict) or set(needs) != EXPECTED:
        raise ValueError("missing or unexpected dependency")
    for name in sorted(EXPECTED):
        result = needs[name]
        if not isinstance(result, dict) or result.get("result") != "success":
            raise ValueError(f"dependency not successful: {name}")
    return {name: needs[name]["result"] for name in sorted(EXPECTED)}


def self_test():
    good = {name: {"result": "success"} for name in EXPECTED}
    assert len(verify(good)) == 39
    name = sorted(EXPECTED)[0]
    missing = dict(good)
    del missing[name]
    variants = [None, [], {}, missing, {**good, "extra": {"result": "success"}}]
    variants += [{**good, name: {"result": status}}
                 for status in ("failure", "cancelled", "skipped", "", None)]
    variants += [{**good, name: None}, {**good, name: {}}]
    for value in variants:
        try:
            verify(value)
        except ValueError:
            continue
        raise AssertionError("invalid dependency receipt accepted")
    print(f"108 gate self-test PASS: 1 positive, {len(variants)} negative")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    if args.output is None:
        parser.error("--output required")
    results = verify(json.loads(os.environ["NEEDS_JSON"]))
    receipt = {"sha": os.environ["GITHUB_SHA"],
               "run_id": os.environ["GITHUB_RUN_ID"],
               "dependencies": results,
               "scope": "CI dependency gate; matrix fault-acceptance aggregates four jobs"}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n")
    print("108 dependency gate PASS")


if __name__ == "__main__":
    main()

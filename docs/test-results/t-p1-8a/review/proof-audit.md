# T-P1.8A storage proof audit

Scope: `tools/storage_site.py`, `tests/test_storage_site.py`, the metadata verification path in `tools/test_storage.py`, and the actual `build/storage-integration` ELF/raw Image. Repository changes authorized by root were confined to the first two files. No Git, remote, QEMU, disk-image mutation, or other repository edits were performed by this auditor.

## Current result

PASS for actual `build/storage-integration` artifacts, independently parsed with Python standard library. The repeatable independent audit is `proof-audit.py`; structured evidence is `proof-audit.json` in this scratch directory. `python3 -m unittest tests.test_storage_site -v`: 12 test methods PASS.

- AArch64 ELF entry and fixed Image start: `0x40080000`; raw first instruction branches to the unique global executable `_start` at `0x40080040`.
- Complete image: `[0x40080000, 0x400ae000)`, 188416 bytes.
- Complete zero-filled BSS: `[0x4008c000, 0x400ae000)`, 139264 bytes, wholly inside one writable identity PT_LOAD with file size zero.
- All three PT_LOAD spans are identity, disjoint, wholly inside the declared image, and completely claimed by the allocated section envelope. All allocated file-backed section bytes match the corresponding raw Image bytes.
- Five image-owned storage objects each have exact complete size 4096 and 4096 alignment; all ten named resources are pairwise disjoint, including diagnostic, original stack, vectors, old boot pool and metadata.
- ELF: 122136 bytes, SHA256 `ced2b3c2c5d79fbc113cf85d0f7021313966c51c5e666a2445ba599d300f0e7c`.
- raw Image: 48368 bytes, SHA256 `16ff606a04897d44dc06e1952d7b5d7af40ab7f2807f0c6dc82f1b295fbb7e7d`.
- Manifest has only the AArch64 metadata map and exactly the two expected artifacts; sidecars contain the same exact compiler flags and ordered list of 15 dedicated sources.
- Independent binary proof passed with `subprocess.run` and `subprocess.Popen` replaced by functions that fail if called. No LLVM tool or instruction-decoding proof is used or claimed for storage.

## Checker hardening performed

Adversarial fixtures initially demonstrated acceptance of contradictory late target/architecture/profile flags, a branch to vectors instead of `_start`, an overlong BSS LOAD, and an extra unrelated LOAD. These are now rejected. Added meaningful fixtures also cover duplicate/local/non-code/incorrect-size entry labels, unclaimed interior image LOADs, overlapping/nonidentity LOADs, malformed alignment and split define/undefine arguments.

This result authenticates finite binary extents and metadata. Physical admission, EL/MMU/cache state, validated DTB source, the persistence protocol and independent VM lifecycle require guest UART plus runner evidence. They are not inferred from the static binary proof.

## UART review status

The existing `build/storage-bringup/run-20261116T214019.938448Z-d6d8jqhm` init-A trace was from the earlier two-flush save protocol. It is correctly rejected by the updated parser at the PREPARED counter check after the explicit inactive-commit invalidation protocol was added. It is not final seven-VM evidence. Final traces are pending the root-run integration.

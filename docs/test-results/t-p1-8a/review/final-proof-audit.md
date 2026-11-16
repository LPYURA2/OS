# T-P1.8A final proof audit

**PASS.** Independent read-only review of both final runs from implementation commit `6dfb61d49835c50d561ca2b4a509563e3c744e59`. No repository changes, QEMU launches, disk writes or Git mutations were performed by this final audit. Read-only Git blob access authenticated source contents against the pinned commit.

## Source, artifacts and complete memory extents

- Both recorded source snapshots contain exactly **110 files**. Every SHA matches the corresponding Git blob at the pinned implementation commit and the current source bytes. Both runs record that commit and `git_dirty=false`.
- Both runs are **16/16 PASS, exit 0**. Four actual new artifact instances were independently hashed, yielding the same two distinct hashes in both runs:
  - AArch64 ELF: 122136 bytes; SHA256 `b730312193d84f8ce4d2795860c799e36c8211d645eb11f90126826baa4b2c25`.
  - raw Image: 48496 bytes; SHA256 `756010c0fa3f41a5506f4d43233042141dab5d96ab67950aae111ceb1f8bbea8`.
- ELF, raw Image, manifest, compiler sidecar, site sidecar and per-VM artifact/manifest SHA bindings agree. Sidecars have the exact ordered 15-source storage list and compiler flags; no other architecture/profile is included.
- Independently parsed ELF entry `0x40080000` and raw branch reach unique executable `_start` at `0x40080040`. Full image is `[0x40080000, 0x400ae000)` (188416 bytes). Complete BSS is `[0x4008c000, 0x400ae000)` (139264 bytes), inside one writable identity LOAD with zero file bytes.
- All 3 LOAD spans are complete, identity, disjoint and inside the declared image. Every allocated file-backed section matches all corresponding raw Image bytes. All ten named resources are disjoint; control, workspace, queue, request and vblk_control each have exact complete size/alignment 4096.
- The proof was independently repeated with external process execution disabled. No LLVM utility is called or integer-instruction decoding claim made by the storage binary proof.

## Fourteen VM traces and persistence protocol

Every raw UART SHA was recomputed, every trace passed the strict parser, and each re-parsed structure equals its complete saved JSON trace. All 14 recorded VM PIDs are distinct. Commands select fixed ARM virt/cortex-a57, one CPU, 128 MiB, TCG, separate boot Image and one writable modern VirtIO MMIO file disk with flush enabled; none uses snapshot/restore.

Observed hardware contract in every VM: EL1 (`CurrentEL=4`), `SCTLR_EL1=0x00c50838` with MMU and caches off, `DAIF=0x3c0`, aligned original-image SP, vectors `0x40080800`; DTB source `0x44000000`, bytes `0x100000`, 32 admitted candidates. The block device is MMIO `0x0a003e00`, version/id 2/2, 2048 sectors, queue 8, negotiated `0x100000200` (VERSION_1 and FLUSH). Image/source/device reservations, object exclusions and candidate complement pass. GRANTED remains zero; there is no general RAM allocation.

Each save performs **three protocol flushes**, in source order: zero inactive commit then FLUSH; metadata and two payload sectors then FLUSH; commit then final FLUSH. Explicit initial superblock initialization adds one format FLUSH. Final counters are exact:

| Phase | Reads | Writes | FLUSH | Requests | Outcome |
|---|---:|---:|---:|---:|---|
| init-a | 2053 | 6 | 4 | 2063 | A committed; includes blank full-disk scan and format flush |
| read-a / read-b / recover-a | 14 | 0 | 0 | 14 | Exact independent file read; disk SHA unchanged |
| update-b | 19 | 5 | 3 | 27 | Distinct B committed and verified |
| prepare-b | 14 | 4 | 2 | 20 | Invalidation and B data flushed; no B commit |

Only the two `prepare-b` VMs contain the validated permanent `BEFORE_COMMIT` barrier. They contain no COMMITTED, PASS or ordinary HALT, and were deliberately stopped with SIGKILL/returncode -9 after observation. The other twelve VMs completed the full PASS/HALT trace, remained alive for observation, and handled intentional SIGTERM/returncode 0. None is recorded as timeout, forced fallback stop, reset or natural exit.

For each run, the persistence disk chains init A → independent read A → update B → independent read B. The separate crash disk chains init A → prepare B/barrier/kill → independent recovery of the exact 15-byte A. Name `note.txt`, generations, lengths and every payload byte match; B is the distinct 24-byte version. Every adjacent disk identity/SHA state agrees byte-for-byte, and current regular single-link disk files match the last recorded identity, length and SHA.

Host source review confirms the runner only creates two fresh exclusive regular zero files; subsequent host disk-state access uses O_RDONLY. Build/host evidence output paths are separate. Guest code constructs every superblock, metadata, payload and commit; the complete disk chains contain no host repair or payload injection.

| Run | Distinct QEMU PIDs |
|---|---|
| `run-20261116T214900.022421Z-ois7fax0` | 13675, 13679, 13683, 13687, 13691, 13695, 13699 |
| `run-20261116T214906.150496Z-rf5q4lxp` | 13767, 13771, 13775, 13779, 13783, 13787, 13791 |

All raw UART SHA values independently checked:

| Run | Stage | UART SHA256 |
|---|---|---|
| 1 | `persist-init-a` | `019fcf41fd674c08a6198d16f313c0dc3d60cfb5beccc267553b7ca803f0da57` |
| 1 | `persist-read-a` | `f6b4b723d93ebadaa012cd2ef8d9973429b3883838fa0b8893607d36181e61f5` |
| 1 | `persist-update-b` | `130d49777d49d3193003e3f4ea098ee1ae3fd5ed5ea43d6c8faf296980f99cec` |
| 1 | `persist-read-b` | `1bd412740ece8341acae73de457fc17efbf30cd9864e66280ad9eb90b7d47a40` |
| 1 | `crash-init-a` | `019fcf41fd674c08a6198d16f313c0dc3d60cfb5beccc267553b7ca803f0da57` |
| 1 | `crash-prepare-b` | `fbd05d42e022f4d1761e9dcc69d695b0e628eaac3eed65a29df047d2efdb3133` |
| 1 | `crash-recover-a` | `2dcb4021ad1888225e6bfdb7d71e446ac824c1e6eb416237f87828f5c9888f1a` |
| 2 | `persist-init-a` | `07373fb9540e4b5a680e64b61716fc88002da6fdd0afd9354f9061eb0afa5fa1` |
| 2 | `persist-read-a` | `42330aa5d99154b504146bec2bc3fe31d7dad9f5f934890b5bbcc0b4e4a1b17a` |
| 2 | `persist-update-b` | `bfaa8a248cd2621b8db7e221c8087aaea99f489d1236228417b6be05d8c7c4f0` |
| 2 | `persist-read-b` | `5f418943d3d99044cd0266a92f967780cfe6787ab71b959c620682bd4cf95bed` |
| 2 | `crash-init-a` | `07373fb9540e4b5a680e64b61716fc88002da6fdd0afd9354f9061eb0afa5fa1` |
| 2 | `crash-prepare-b` | `fbd05d42e022f4d1761e9dcc69d695b0e628eaac3eed65a29df047d2efdb3133` |
| 2 | `crash-recover-a` | `3127f233d78aaead5a7aaeca429bd45766519f175b90ddeda1a76bd8826b6db6` |

## Preserved behavior and repeated-run evidence

- Independently rehashed and byte-compared **44/44** actual previous-profile artifacts against `baseline-build`; all agree with the recorded baseline hashes and contain no new storage marker.
- Recomputed all **59** preserved first-run file hashes after the second same-output invocation; all still match the pre-repeat inventory.
- Independently read full-regression results: **57/57 PASS**, with **261 Python tests**; ARM-only: **43/43 PASS**, with **261 Python tests**.
- Ordinary attached-writable-disk guard passes: raw UART SHA matches; ordinary halt occurs with no STORAGE marker; the attached file is still the entirely blank 1-MiB SHA before and after.

These are Linux QEMU process-lifecycle and file-disk observations. They do not establish Android/Termux T-P1.8A, host/phone power-loss behavior or physical-device durability.

Full structured details, exact flags/sources, object extents, all VM states and artifact hashes: `final-proof-audit.json`. Repeatable audit: `final-proof-audit.py`.

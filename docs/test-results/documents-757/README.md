# Stage 757 — host evidence, 21 Nov 2026

Command: `sh tools/test_system_apps_757.sh`
Host: Linux environment with Python 3, GCC, Clang and ld.lld.

- Existing stage-756 app regression: PASS (legacy Python suite 43, 2 skipped
  because old ISO not mounted in their default fixture path; older C tests
  on GCC/Clang and Clang sanitizers PASS; real Linux x86-64 ELF entry/event/draw
  and TXT/DOCX PASS; 1200 negative DOCX samples PASS).
- Standalone system-app preinstall tests: 29/29 PASS. These 29 are included
  in the 43 automatically discovered by the legacy test runner; they run
  again in the explicitly named stage-757 suite. Do not add counts together.
- Development preinstall bundle reproducibility: PASS.
- Bundle corruption/identity/ELF checks: PASS.
- Host system-folder staging and catalog verification: PASS.
- Repeated install and rollback with preserved user files: PASS.
- Deliberate production install refusal (missing trusted signature/native ABI): PASS.
- Control 744 ISO SHA-256: PASS, `011dd3560e8d232b00201623443d9a09b0c67e03227b66504312a31f75e285ae`.
- Built `Documents.tea`: 30,384 bytes, SHA-256 `49915e76bf20daa0981a3aa842d2322bfe6a8690a6883e0d0e4c014de96666e8`.
- Dev preinstall bundle: 33,888 bytes, SHA-256 `569d5571c7f4f1b9371fd598757c98a41b0693872fbca02daa5ccf5354958d5e`.

**NOT RUN / NOT PASS:** new bootable ISO; QEMU; VirtualBox; guest
installation; guest application execution; publisher signing; all 14 UI
screens. These cannot be assumed from host checks.

Full local test transcript: `stage757-full-tests.log` in the delivered source
archive. No prior branches or user data disks changed.

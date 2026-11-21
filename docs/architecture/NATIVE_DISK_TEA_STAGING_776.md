# Stage 776: shared pinned disk-.tea staging, not a desktop process launch

Base commit: `ef4b2370dacf85db74df9f37e82a37c76dd584b2`
(775, GitHub Actions 44952900969 PASS).
Branch: `feature/styleos-native-disk-staging-776`.

## Problem and narrow change

The 769 QEMU guest had bespoke disk parsing and ELF page staging, while the
775 ordinary Desktop had an isolated process/session/window/IPC transaction.
There was no single reusable, bounded entry point for the disk-to-executable
preparation step. Stage 776 extracts that exact read-only step into
`kernel/native_tea_staging.c` and `include/styleos/native_tea_staging.h`.

The kernel-private `nts_prepare` takes a **trusted** sector-read callback,
a pre-approved canonical package/ELF SHA-256 pin and exact manifest, and
caller-owned scratch/package/code buffers. It rejects malformed headers,
short media, read errors, package hash mismatch, ZIP structure/CRC mismatch,
ELF mismatch, overlapping buffers, non-aligned code page and oversized
contents, **before** publishing a result. On any refusal the executable page
and output descriptor remain unchanged; scratch storage may contain untrusted
bytes and must not be executable. There is no write command to disk.

The old 769 experiment now directly calls `nts_prepare` and prints the extra
`STYLEOS TEA776 SHARED STAGING PASS` UART marker only on success. Both the
ordinary Desktop build and the disk-CPL3 experiment link the **same** source
module; the normal Desktop does not invoke it at boot. The 769 hardware
validator explicitly requires the new marker and still tests intact package
execution, wrong-CRC/digest refusal, PID/CPL3 traces and exit, plus an
unchanged backing disk.

## Trust model and safety

- SHA-256 pinning proves equality to a separately approved test package,
  **not author authenticity** or package signatures; the test fixture is not
  a general installation policy.
- The caller must separately prove that every buffer is backed by reserved,
  kernel-owned physical frames, with the right lifetime and mappings.
  `nts_prepare` cannot allocate physical pages or establish this proof.
- The trusted sector callback must not be reentrant; no locking, SMP or
  outstanding task cancellation is supplied. The caller may not publish the
  code page to a user until all verification completes.
- Output ELF support is the one-page, fixed-entry x86-64 diagnostic from 768.
  This is not a full ELF loader, relocator, signed package manager, ASLR or
  generic `.tea` interpreter.
- No code switches CR3 in the ordinary Desktop. No native app PID, session,
  window, IPC port or user is created at ordinary boot. `native_desktop_launch`
  (775) remains a separate, dormant admission coordinator.
- The developer/CI controlled test disk is mounted as an **IDE snapshot** in
  QEMU; both valid and tampered source disk images must remain unchanged.

## Verification and next dependency

Mandatory check: `python3 tools/test_native_tea_staging.py --output ...`.
This compiles the canonical `.tea` fixture, runs the shared C module with
ASAN+UBSAN against actual disk bytes, exercises refusal and no executable
write on error, and performs a strict x86-64 COFF compile.

The workflow also compiles normal Desktop UEFI and experimental disk UEFI,
executes all earlier host/OVMF regressions and runs
`tools/test_native_tea_769_guest.py` in real QEMU. The positive case proves
the newly shared staging code is executed on the physical IDE read path and
the resulting code enters CPL3 and exits; the negative case proves tamper is
refused before CPL3.

**Next stage 777**: build an independently attested *runtime* memory authority
and connect the common package staging to the 775 launcher and actual syscall
callbacks. First require safe root closure, fresh physical frames, user entry
mapping and per-session authority; then prove Desktop and native app run
concurrently in QEMU, including input/output and fault cleanup. Do not claim
766–776 host assertions alone prove simultaneous GUI isolation.

After successful independent apps: approved login chooser and real user
accounts → StyleFS per-user homes/rights/recovery → Explorer completion.
Keep the existing desktop visual design and `Быстрый доступ` naming.

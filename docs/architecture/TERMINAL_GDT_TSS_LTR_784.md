# Stage 784 — terminal x86-64 LGDT/LTR/STR experiment (not Desktop)

## Accepted starting point and branch policy

Base: `04b41fa73b778eb93bcf5d796ea5830fc9df6544`, stage 783, GitHub
Actions run `45023164821` SUCCESS. The new branch is
`feature/styleos-tss-lgdt-ltr-trial-784`. Neither `main`, the previous 783
ref, nor the ordinary Desktop test profile is modified by this experiment.

## Intended hardware chain

This is a dedicated `--test-tss-ltr-784` **test PE** run only under the QEMU
`isa-debug-exit` device. It shares the proven 779–783 testing path: old CR3
→ temporary image-owned CR3 → verified old CR3 restoration → read-only live
PE/IDT/boot-stack inventory → four individually mapped image-owned pages
(GDT, TSS, RSP0, IST1) → a fresh *inactive* 16-byte available TSS descriptor.
The new pure preflight refuses nonzero TR, enabled IF, unproven page/IDT
access, malformed TSS/GDT, improper GDTR/IDTR, stale CS and any existing
IST gate. Original GDT bytes and inherited CS are preserved. The early IDT
has 256 present interrupt gates with IST=0, all pointing to the current CS;
the 781 hardware check has already verified their page mappings under the
restored old CR3. Stage 784 also re-attests the new GDT/TSS pages as
supervisor-only, writable, WB and identity-mapped.

Only after all preconditions pass, a narrow terminal routine with IF cleared
executes **LGDT**, performs **SGDT** readback, then **LTR** of the staged
selector. Real **STR** readback must match, and the new in-memory GDT TSS
descriptor must have transitioned from `available (0x89)` to `busy (0x8B)`.
CR3, CS and IDTR are rechecked. The test records a final UART marker and
exits the entire VM via QEMU ISA debug-exit at port `0xF4` with value `0x10`
(expected process status `33`). Even a check failure after LGDT uses a terminal
error exit (`35`); it **never returns** to ExitBootServices, UEFI, Desktop or
an old GDT/TR state. No `LTR(0)` is used or claimed to restore the firmware.
Unexpected CPU faults fall through the existing fail-stop early IDT, not a
success marker. Such a fault fails the mandatory independent QEMU verification.

## Explicitly not demonstrated

- No hardware IST exception delivery, guard-page PTEs, stack overflow or
  nested-fault survivability. `IST1` is an encoded pointer only.
- No production descriptor installation, runtime TR restoration, exception
  containment/recovery, OS-owned CR3, PMM, IRQ, SMP or per-process TSS.
- The inherited PE memory remains RWX; usable modern W^X memory policy and
  complete hardware/firmware mappings remain blocked by 779/780/781.
- No native `.tea` processes running concurrently with normal Desktop, and no
  changes to approved desktop UI, task manager or apps.

## Verifiable evidence and refusal

`tools/test_tss_ltr_784_host.py` uses ASAN/UBSAN, COFF, and mandatory
`llvm-objdump` to check the real instructions. It checks the ordinary Desktop
binary for absence of trial marker; `tests/test_tss_ltr_784.c` contains
negative cases for descriptors, selectors, IF, IDT gates and frame evidence.
`tools/test_tss_ltr_784_verify.py` exercises adversarial UART parser cases on
**synthetic fixtures only**; these do not count as hardware acceptance.
`tools/test_tss_ltr_784_guest.py` boots a separate real x86 QEMU/OVMF process,
checks the precise port-triggered exit code, and binds 784 UART values to the
actual 781/782/783 hardware records and UEFI image extent. On timeout,
software-only evidence, forged STR, busy bit, IST success or firmware return,
this step is FAIL. The mandatory prior real-QEMU stage 783 check is retained.

**Status**: Acceptance requires the exact final commit and a SUCCESS complete
GitHub Actions run whose 784 QEMU job passes. Before then the branch is a
candidate, not a confirmed hardware stage.

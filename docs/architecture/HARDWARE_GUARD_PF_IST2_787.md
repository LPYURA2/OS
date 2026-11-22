# Stage 787 — actual guard-page `#PF` using IST2 (terminal-only)

**Starting point:** accepted stage 786 `b6eab7b883694a5a255b16be59eaa443754430f3`.
**Task branch:** `feature/styleos-hardware-guard-pf-787`.

## Experimental design (strictly off Desktop)

The isolated `--test-guard-pf-787` profile reuses the successful 786 setup and
attested inherited CR3 walk, but has **ten** separately image-owned 4 KiB
frames, adding an independent IST2 backing frame after the nine 785 frames.
No candidate RAM is granted/reclaimed. In the cloned root at PML4 slot 128,
the PT leaf index 3 maps IST1 at `0x0000400000003000` and leaf index 6 maps
IST2 at `0x0000400000006000`. Both are P|RW|NX supervisor pages.
Indices 2,4,5,7 are **exactly zero** guard PTEs, including index 2 (the
first unmapped page just below IST1). The new IST2 frame is independently
attested for identity and write permissions in inherited CR3. All inherited
firmware and image mappings remain: **no production kernel CR3/W^X claim**.

The experiment composes GDT/TSS from the verified 783 routine, then writes
TSS.IST2 (offset 44) only after the 783 strict TSS verification. IDT[3]
uses IST1 and IDT[14] uses IST2. After one-way `mov CR3`, `LGDT`, `LTR`,
`LIDT`, the true `INT3` ISR enters on IST1 and independently validates the
saved processor RIP/CS/RFLAGS/previous RSP. The running IST1 handler then
writes to its lower **unmapped** guard page. A real CPU `#PF` must switch
onto IST2, and only its genuine ISR (with the processor-supplied error code)
can emit the terminal PASS record and issue QEMU `isa-debug-exit` port 0xf4.
Any `INT3` return, a mapped guard, bad context, fault, hang, reset, missing
UART sequence or exit code other than host return 33 must FAIL.

Hardware verification checks `CR2==0x0000400000002000`, the **exact PFEC 2**
(P=0/W=1/U=0/RSVD=0/ID=0), saved RIP equals the labelled *actual MOV*,
old RSP points into IST1, new RSP into IST2, CS/TR and CR3 match,
leaf 3/6 have hardware A|D while the four neighbor guard PTEs remain zero,
and all records are in order. Firmware is NEVER called after `LTR`.

## Acceptance and limits

This document is a **design description**, not a CI result. To accept 787,
inspect the **exact final SHA**, successful full workflow, independent UART,
`results.json` and QEMU log, plus immutable Desktop EFI comparison with
accepted 786. Host negative parser tests alone cannot establish hardware
success. The negative test also rejects forged CR2, PFEC, saved RIP, IST2,
PTE permissions, unexpected exit codes and duplicate markers.

Not proved: IST1/IST2 stack *exhaustion* recovery, `#DF` nested-fault
handler, re-entering user mode, SMP, production page-table ownership,
complete inherited MMIO/firmware closure, safe W^X for the whole kernel,
normal Desktop sharing hardware with separately installed `.tea`, or
filesystem/account permissions. This is a terminal single-core test.

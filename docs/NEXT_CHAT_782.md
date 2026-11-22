# StyleOS — continuation point after stage 782

**Repository:** LPYURA2/OS.

**Branch:** feature/styleos-tss-ist-reserved-staging-782.
**Exact commit / GitHub Actions:** use completed run and SHA; verify before claiming acceptance.

## Verified scope

Four independent pages for candidate GDT, TSS, RSP0 and IST1 are reserved
in the dedicated hardware test PE image. The newly linked
kernel/tss_ist_stage_782.c validates they don't alias the already proven
image-owned pages, the live boot stack, or any candidate RAM. Four individual
inherited-CR3 identity, supervisor, writable/WB hardware walks precede staging.
A 104-byte candidate TSS stores RSP0, IST1 and the denied I/O map offset.

**Not done:** no LTR, no active TSS/IST or guard pages, no own complete CR3,
no .tea beside the normal Desktop. The ordinary Desktop image is unchanged.

Read docs/architecture/TSS_IST_RESERVED_STAGING_782.md and previous stage
781/780/779 documents. Use new branch **783** for guarded GDT/TSS context and
then an independent QEMU LTR/restore experiment. A test-only prepared structure
is not a kernel subsystem.

Long-range order unchanged: genuine .tea → Process Panel → login by mockups
→ users + StyleFS ACLs → Explorer by mockups (Быстрый доступ) → actual
transactional .tea installer → App World; network and sound afterwards.

Rules: separate GitHub branch per stage; don't modify main or accepted refs,
retain the polished Desktop and working apps; don't invent PID, CPU/RAM or
hardware evidence. Deliver exact CI full source, boot ISO, QEMU evidence,
complete ZIP and next-chat handoff for each accepted stage.

# Stage 788 — terminal hardware `#DF` on IST3 after nested exception

Stage branch `feature/styleos-hardware-double-fault-788` from **accepted** stage 787,
SHA `8b8ea23acd7718c85eb20053c1ce5b4b6237545c`.

## Scope and mechanism

Only `--test-double-fault-788` uses these objects. Normal Desktop and earlier
profiles exclude the `double_fault_trial_788.c` object, reserve no related BSS
and never enable the injected fault. No accepted branch or `main` is changed.

After stage 785 structural audit, the test extends its image-owned staged page
tables with a separate IST2 and IST3. IST1 has PTE index 3, IST2 index 6,
IST3 index 9, each writable, supervisor-only and NX. PTEs 2, 4, 5, 7, 8,
and 10 must remain absent. Frame ownership is restricted to admitted reserved
loaded-image memory; no candidate RAM grants, no general allocator.

Stage 783 GDT and TSS are generated and validated *before* the additional
TSS IST2 (offset 44) and IST3 (offset 52) fields are installed. IDT vector 3
is an interrupt gate on IST1; vector 8 is a present interrupt gate with IST3;
vector 14 is intentionally nonpresent. Hardware INT3 enters IST1. Handler
attempts a supervisor write to the IST1 lower unpresent guard. The ensuing
`#PF` cannot be delivered through the nonpresent vector 14 gate; the processor
must generate `#DF` with error code zero and enter the vector 8 handler on IST3.

Handler must independently check live CR3/TR, IDTR/GDTR, CPL0, CR2, zero
`#DF` error, faulting RIP equal to the guarded MOV label, previous IST1 RSP,
new IST3 RSP, TSS IST3 pointer, a live non-present `#PF` gate and present
IST3 `#DF` gate, distinct image-owned frames, active leaf entries and
surrounding guard PTEs. No IRET, firmware return, LTR(0), IRQ re-enable,
boot continuation or test-state restoration; success terminates QEMU via
isa-debug-exit only. Any unexpected return, hang, hardware reset or wrong exit
must fail. The parser rejects additional/malformed/contradictory UART markers.

## Limits

This is a *controlled nested exception from an intentionally disabled `#PF`
gate*, **not** physical stack exhaustion, safe rollback after kernel stack
overflow, recovery from a real production `#DF`, validated SMP, interrupts,
full W^X or driver/firmware mapping closure. This test cannot prove the latter.
The new root still inherits most existing PML4 entries and dependencies.

## Acceptance evidence

CI must provide unmodified UART, QEMU process status (expect 33), output of
`double_fault_788_verify.verify`, negative parser tests, and passed inherited
host/QEMU regressions. Run-specific SHA/results belong to the final report;
never mark a new stage PASS based on source compilation or synthetic logs.

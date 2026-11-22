# StyleOS — continuation after stage 784

Repository `LPYURA2/OS`; base 783 `04b41fa73b778eb93bcf5d796ea5830fc9df6544`
(CI `45023164821` PASS). Stage 784 branch:
`feature/styleos-tss-lgdt-ltr-trial-784`.

**Before acceptance**: retrieve the exact final SHA and complete GitHub
Actions run, including the dedicated real QEMU LGDT/LTR/STR and debug-exit
step. Do not claim 784 PASS from host or COFF tests alone.

Stage 784 introduces a terminal, dedicated `--test-tss-ltr-784` PE. It does
not change ordinary Desktop. After physical mapping/IDT preflight, it loads
the staged GDT via LGDT, executes LTR, checks STR and TSS busy-bit, and
terminates QEMU without UEFI return. Failures, hangs and faults must not be
counted as successful transitions. The previous diagnostic `--test-frame-grants`
retains the no-LTR behavior of 783.

Remaining: no guard PTEs, no IST fault/delivery acceptance, no safe independent
production CR3, no complete kernel W^X / device dependency closure; no
concurrent independent `.tea` alongside ordinary Desktop. Do not issue a
production TSS readiness claim. Avoid LTR(0) to restore TR=0.

Next work subject to owner approval: make guarded, individually owned
supervisor stacks and fault-safe IDT paths, and prove hardware IST entry and
recovery in a **separate stage** before integrating independent native `.tea`
with Desktop. Later: Processes; login per approved mockups; users and StyleFS
rights; Explorer (`Быстрый доступ`); atomic `.tea` installer; App World;
network and sound later.

Rules: every stage new branch, never alter main/accepted branches, preserve
polished Desktop. Deliver complete CI source ZIP, CI ISO, evidence ZIP and
exact SHA/run continuation. Read AGENTS.md, docs/PROJECT.md, this file and
`docs/architecture/TERMINAL_GDT_TSS_LTR_784.md` first.

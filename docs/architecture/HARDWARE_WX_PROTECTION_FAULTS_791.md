# StyleOS 791 — terminal hardware write-protect and NX faults

Base: accepted 790 `2885ef6da34bc00693e0d61e2bdd80b0bdde4bad`.
Branch: `feature/styleos-hardware-wx-pf-791`.

Two independently built, terminal-only x86-64 images use the 790-owned low
PDPT/PD/PT hierarchy and dedicated image-owned IST1, IST2, IST3. `--test-wx-write-791`
writes to the actual `.text` RX address; `--test-wx-exec-791` calls into a
known RET byte held in `.data` RW/NX. Both must generate a genuine CPU #PF
on IST2, with exact CR2, PFEC, saved faulting RIP and previous IST1 RSP.

Write policy expects PFEC=3 (present + write + supervisor) with CR0.WP=1.
Instruction-fetch policy expects PFEC=0x11 (present + execute fetch) with
EFER.NXE=1. Both check the true active CR3, TSS TR, permissions, TSS IST2
frame, source page role and IDT #PF gate. An execution or write that succeeds,
unhandled #DF, reset, timeout, UART fabrication or incorrect exit code is FAIL.
The inherited firmware low tables are not used after terminal CR3 switch.

The test binary never returns to UEFI, never restores old CR3, and never
uses LTR(0). The ordinary Desktop image and earlier accepted branches
are not modified. **These probes do not establish production W^X across all
kernel/device stacks, interrupts, firmware runtime, user processes or SMP.**
This is no claim of independent native `.tea` applications co-running with
Desktop. Require both real QEMU logs, strict independent negative parser tests,
CI success, source and ISO artifacts before accepting 791.

After 791: enumerate hardware IRQ, MMIO/GOP, firmware runtime and all kernel
stacks under the owned root; then production W^X integration in separate
reviewed stages. Owner plan: Processes, shell-independent `.tea`, user login,
StyleFS rights and Explorer per approved PDFs, installer/App World, network/audio/VPN.

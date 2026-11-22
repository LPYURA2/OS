# StyleOS 790 — owned low-half tables with page-level W^X intent

Branch `feature/styleos-owned-low-tables-wx-790`; base accepted 789
`6914ae2c3e83bce437f297a7f53ea9168eb3725a`.

## Scope (experimental terminal EFI only)

`--test-owned-low-790` prepares a completely image-owned, 4-KiB-leaf low
PML4[0] hierarchy. The active PML4[0] points to the image-owned PDPT built
by `pg_build`, not the UEFI PDPT. Only three kinds of low mapping survive:

- the loaded PE image, mapped by its *loaded* RVA section permissions:
  executable RX, writable RW/NX, headers and read-only RO/NX;
- the existing boot stack (RW/NX); and
- no other inherited low-half identity mapping, including no inherited old
  page-table pages, firmware GDT, IDT or firmware services.

PML4[128] retains the separately image-owned three-stack IST1/IST2/IST3 alias
hierarchy and its absent guard pages. There are exactly two nonzero PML4
entries. No arbitrary MMIO or framebuffer is mapped. After switching CR3,
interrupts stay disabled and the experiment uses only I/O-port COM1 UART,
owned GDT/TSS/IDT, owned code and image-owned data. It never calls firmware
and never returns or attempts an LTR(0).

## Fail-closed staging

The builder requires a valid PE32+ image with exact loaded `SizeOfImage`, a
bounded section table, no 4-KiB page that mixes executable and writable
permissions, no uncovered image page, non-overlapping boot stack, and a
physically attested 32-page image BSS table reserve. It rejects malformed
headers, ambiguous flags, root/VA overlap, insufficient table capacity and
missing mandatory RX/RW stack addresses. Only after constructing and auditing
all identity leaves does the hardware driver replace root slot 0; the
inherited root is never modified. Accessed/Dirty are the only allowed CPU
modifications to leaf permission bits after activation.

`CR4.PGE` is cleared before the terminal MOV CR3 to flush old global TLB
translations; `CR0.WP` and `EFER.NXE` must be enabled. No PCID/LA57.

## Hardware acceptance

The terminal QEMU/OVMF test must produce its real UART log with measured old
and new root links, owned low-table base and number of pages, PE RX/RW/NX
counts, boot stack rights and separate IST alias links, followed by real
`INT3 -> IST1 -> absent guard #PF -> not-present #PF gate -> #DF IST3`.
A strict independent parser verifies full hardware frame, CR2, zero #DF
error, exact RIP, TR, active PML4 links, independent low PDPT identity,
correct image page-rights census, QEMU exit code 33 and no firmware return.
The parser's synthetic fixture is ONLY a negative-test fixture.

## Unproven or intentionally excluded

- No production Desktop CR3 integration, native .tea co-execution, scheduler,
  IRQ or multi-CPU handoff under these tables.
- No framebuffer/GOP, device MMIO, general RAM/heap ownership, universal
  kernel-stack closure, firmware runtime access, cache/PAT closure or SMP.
- Actual hardware enforcement of NX/RO *faults* outside the chosen guard
  path requires separate negative-execution and write-protect probes; table
  entries and code execution alone do not prove every kind of fault.
- Double-fault handling here is a terminal diagnostic, **not** recovery
  from exhausted kernel stacks or arbitrary faults.
- Firmware is still responsible for initial loading and the pre-handoff
  boot stack. It is the *page-table hierarchy* beneath slot 0 that is now
  detached in this experiment, not every historical dependency of the OS.

## Acceptance and next stage

Require GitHub Actions SUCCESS on the exact final SHA, host ASAN/UBSAN and
COFF compile, malformed-PE refusals, negative UART parser, live guest QEMU
`results.json`+`serial.log`, QEMU debug-exit 33, unchanged ordinary Desktop,
complete source ZIP, ISO and evidence artifact. Until CI succeeds this stage
remains a candidate.

Next should prove real write fault on an RX page and real execute fault on
an NX page with safe dedicated IST handling; after that design production
kernel/device stack closure prior to enabling independent .tea with Desktop.

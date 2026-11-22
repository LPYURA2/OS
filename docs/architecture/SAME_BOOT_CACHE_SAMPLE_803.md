# Stage 803: one QEMU boot PAT/MTRR/PTE audit

## Scope and explicit non-goals

This is a **terminal, read-only test EFI**, not the ordinary StyleOS desktop.
It does not change `CR3`, `IA32_PAT`, MTRRs, PCI BARs, page tables or MMIO data.
The normal image, UEFI `boot_info` v1, graphics configuration and applications
remain unchanged. The test cannot establish global MMIO alias absence, firmware
runtime safety, DMA/IOMMU protection, IRQ/APIC routing or readiness of production
page tables. `production_cr3_authorized=false` is unconditional.

## What is sampled

A single QEMU/OVMF boot observes `CR0`, `CR3`, `IA32_PAT`, `IA32_MTRRCAP`,
`IA32_MTRR_DEF_TYPE`, all enabled variable MTRR pairs (up to 16), the GOP
framebuffer physical address and the BAR1/BAR4 addresses of downstream VirtIO
Serial. It then uses the existing S800 MTRR evaluator and S801 physical-table
reader to inspect those same 3 numeric VAs from the **same** inherited UEFI
page tables. No target MMIO is dereferenced.

The table walker restricts page-table physical reads to low firmware-described
RAM, as in S801. Unknown or inaccessible page-table dependencies result in
FAIL, not optimistic inference. The independent Python UART verifier checks
all MTRR register values, indices, the sampled PTE leaf entries, their physical
translations, PAT selection, QEMU exit code 33, line ordering, and full denial
policy; for the tested MTRR UC samples the derived sample type is UC.

## Boundaries

- The 3 identity-address samples are **not** a scan of all virtual aliases.
- One-boot read-only observation is stronger than cross-boot S802 correlation,
  but still not a proof against concurrent firmware/hardware changes in general.
- PAT/MTRR result UC is scoped only to each sampled address in this fixed QEMU
  configuration; never generalize to every byte of every BAR or any other machine.
- Only QEMU/OVMF in CI establishes hardware execution. Host fixtures do not.
- The normal desktop is expected to be binary-identical to accepted S802 when
  built in the same CI toolchain; verify by SHA256, do not infer from source.

## Acceptance and next gate

CI must run S803 16 negative parser cases, real `qemu-system-x86_64` with OVMF,
retrieve raw UART and `results.json`, preserve the inherited hardware tests,
fail closed when any dependency is missing, and publish exactly committed source.
Next gates remain global alias/range analysis, IRQ/APIC, DMA/IOMMU, UEFI runtime,
and an independently accepted transition to native production CR3 with the
ordinary desktop/app/file integration suite.

# StyleOS stage 804 handoff

- Parent accepted stage: 803 (`7d7d9d0831aea0e71acf812f19436b1d48592ec3`).
- Branch: `feature/styleos-known-mmio-extent-804`.
- Stage 804: test-only read-only audit of 1005 4KiB pages spanning three known
  fixed-QEMU MMIO ranges, with same-boot MTRR/PAT/pagewalk, UART independent
  validation, and unconditional production CR3 denial.
- Do not confuse GOP pixel surface (4,096,000 bytes) with complete VGA aperture
  (16MiB) or measured BAR sizes for other PCI devices. No global alias scan.
- Check current CI run and exact committed SHA before claiming acceptance.
- Next 805: bound alias or IRQ/APIC evidence; then DMA/IOMMU, UEFI runtime,
  kernel-owned production CR3 and ordinary Desktop/.tea/file integration.
- Keep approved StyleOS UI and intact accepted branches, .tea structure,
  project documentation and native Desktop.

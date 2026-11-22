# StyleOS 797 — PCI MMIO forwarding BASE audit

Base 796: c82245fa940b08644e9f1b0d5b0dc39930b8f454. Branch: feature/styleos-pci-forwarding-audit-797.

Stage 796 observed real QEMU/OVMF PCI topology in two configurations, including a downstream VirtIO device behind a PCIe bridge. Stage 797 introduces a mandatory fail-closed analysis of those **same-run original hardware UART logs** and their SHA-256 results. It does not introduce a new EFI program, and it does not change Desktop.

The validator reconstructs the parent bridge chain for each downstream memory BAR and requires its BASE to fall in the correct prefetchable or non-prefetchable forwarding window on every ancestor. Missing, cyclic or ambiguous routes, wrong windows, out-of-range bases, malformed/forged UART and wrong QEMU exit are rejected. The bridge scenario must contain two independently observed routed memory BAR bases; baseline must have zero bridges.

**Limits:** A BAR base inside a forwarding window is not evidence that the **full BAR aperture** fits. The known 795 VGA BAR sizing is not generalized. Downstream I/O-port BAR forwarding is unverified. IRQ/APIC, PAT/MTRR effective caching, DMA/IOMMU, MMIO register ownership and UEFI runtime remain unresolved. Production CR3 is forbidden.

The two existing QEMU tests run afresh in this CI before the 797 audit, which checks their results/hash. Negative tests are host-only and must never count as hardware evidence. Next: full-range MMIO/PCI ownership and effective cache policy, then integration into ordinary Desktop with hardware validation. Foundation unfinished.

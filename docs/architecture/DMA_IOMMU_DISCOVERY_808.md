# StyleOS 808 — DMA/IOMMU discovery and fail-closed policy

The accepted S807 hardware IRQ test **does not imply DMA isolation**.
S808 is an independent read-only x86-64/OVMF terminal EFI profile.
It captures ACPI RSDP/XSDT and DMAR before `ExitBootServices`, validates
signatures, checksums and bounded lengths, and records DRHD translation units,
optional RMRR reserved ranges, PCI device scope counts and unknown entries.
No IOMMU or PCI device registers are changed, no DMA is initiated, and the
original `boot_info v1`, Desktop, and native `.tea` remain unchanged.

## Hardware acceptance

The CI must run Q35/OVMF **without** an IOMMU and **with** `intel-iommu`.
It rejects QEMU timeouts, missing required DMAR in enabled mode, duplicate
records, forged UART, false DMA protection and altered output semantics.
Input data comes from actual QEMU serial output, with real exit code 33.

DMAR is a firmware **description** of remapping units, not proof that Intel
VT-d translation is enabled or that a device is isolated. An `include_pci_all`
DRHD is a firmware routing claim, not an enforceable OS policy. RMRR ranges
must be explicitly protected before a DMA domain is safe. Unknown unit types
are reported but not silently trusted. Even a correct DMAR parser is NOT a DMA
engine or IOMMU driver; source IDs, device scopes, page tables, invalidation,
remapping enable, and fault handling remain unverified.

## Required restrictions

- `iommu_translation_enabled=false` and `dma_isolation_verified=false`.
- `dma_access_authorized=false` and `production_cr3_authorized=false`.
- Do not enable bus mastering on an untrusted device as a shortcut.
- No production memory-map switch, changes to Desktop, main branch, or original images.
- Future S809: validate actual DMA-remapping unit MMIO registers and device
  source identifiers in another terminal hardware mode, and UEFI runtime later.
- Foundation accepted only after regular kernel/desktop/`.tea`/file operations
  pass on a safe own CR3 with all prerequisites closed.

# StyleOS stage 809 — Intel VT-d MMIO register observation

A DRHD base discovered in ACPI S808 is an **address of a DMA-remapping hardware unit**, not evidence of DMA protection. A separate terminal EFI uses the validated DMAR capture from before ExitBootServices to identify the single QEMU DRHD. It first performs a bounded, read-only 4-level page-table walk of the DRHD MMIO page and requires the inherited UEFI mapping to translate the expected physical address. If absent, this is a hard FAIL, never an unsafe MMIO read.

In QEMU/OVMF both modes (with and without virtual `intel-iommu`) are mandatory. Without a DMAR table, **no** VT-d MMIO reads occur. With a table, the diagnostic reads VT-d Version, CAP, ECAP, GCMD, GSTS, RTADDR and FSTS by read-only MMIO. Repeated read of immutable registers detects instability. `GSTS.TES` and root-table status are observations only; they do not demonstrate safe DMA device ownership or any initialized StyleOS domains.

Limits: no DMA is initiated, no IOMMU registers are written, no root/context/domain tables are configured, no fault handling or invalidation is exercised, requester IDs and remapping scope are not proven. The current diagnostic runs **after ExitBootServices** and ends with QEMU debug-exit. `production_cr3_authorized=false` and `dma_isolation_verified=false` even when hardware registers are readable. The normal Desktop EFI and ISO must be unchanged.

Next: validate device requester ID/scopes and DMA-domain fail-closed design, fault paths, UEFI runtime lifecycle and eventually integration with regular Desktop/kernel and own CR3. Do not prematurely claim the foundation complete.
